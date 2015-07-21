#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>

#include <atomic>

#include <pd/base/mutex.H>
#include <pd/base/config.H>
#include <pd/base/exception.H>

#include <pd/bq/bq_util.H>

#include <phantom/module.H>
#include <phantom/io_benchmark/method_stream/source.H>

#pragma GCC visibility push(default)

#define TAG_LEN (256)

namespace phantom {
namespace taurus {

using namespace io_benchmark::method_stream;

MODULE(taurus_source);

struct schedule_t {
	enum type_t {
		SCHEDULE = 0,
		START_LOOP,
		STOP
	};

	uint32_t type_and_delay_time_ms; // type in highest byte
	uint32_t payload_len;
	uint64_t payload_start;
};

class taurus_source_t : public source_t {
public:
	struct config_t {
		string_t ammo, schedule;
		interval_t max_test_duration;

		config_t() : max_test_duration(interval::inf) {}

		void check(in_t::ptr_t const &ptr) const {
			if(!ammo)
				config::error(ptr, "ammo is required");
			if(!schedule)
				config::error(ptr, "schedule is required");
		}
	};

	int fread_or_throw(void* ptr, size_t size) const {
		int ret = fread(ptr, size, 1, schedule);
		if(ret == 0 && ferror(schedule)) {
			throw exception_sys_t(log::error, errno, "fread: %m");
		}
		return ret;
	}

	bool read_schedule(schedule_t* sched, interval_t* interval_sleep) const {
		mutex_guard_t guard(mutex);

		int ret = fread_or_throw(sched, sizeof(*sched));
		if(ret == 0) {
			if(loop_offset >= 0) {
				log_debug("seeking to loop start at %ld", loop_offset);
				if(fseek(schedule, loop_offset, SEEK_SET) == -1)
					throw exception_sys_t(log::error, errno, "fseek: %m");
				fread_or_throw(sched, sizeof(*sched));
			} else {
				return false;
			}
		}

		interval_t delay = interval::millisecond * (sched->type_and_delay_time_ms & 0xFFFFFF);
		timeval_last += delay;
		*interval_sleep = timeval_last - timeval::current();

		int flag = (sched->type_and_delay_time_ms >> 24) & 0xFF;
		switch(flag) {
		case schedule_t::START_LOOP:
			if((loop_offset = ftell(schedule)) == -1)
				throw exception_sys_t(log::error, errno, "ftell: %m");

			loop_offset -= sizeof(*sched);
			log_error("start loop at %d", loop_offset);
			break;
		case schedule_t::STOP:
			stop = true;
			break;
		default:
			break;
		}

		return true;
	}

	virtual bool get_request(in_segment_t &request, in_segment_t &tag) const {
		try {
			schedule_t sched;

			if(stop) {
				log_debug("stop flag encountered, stopping");
				return false;
			}
			if(timeval::current() > deadline) {
				log_debug("test duration exceeded, stopping");
				return false;
			}

			interval_t interval_sleep = interval::zero;
			if(!read_schedule(&sched, &interval_sleep)) {
				log_error("end of file, stopping");
				return false;
			}

			string_t::ctor_t ctor(sched.payload_len);
			for(size_t i = 0; i < sched.payload_len; ++i) {
				ctor(' ');
			}
			string_t payload(ctor);

			size_t size = 0;
			while(size != sched.payload_len) {
				uint64_t offset = sched.payload_start + size;
				int ret = pread(ammo_fd, (char*)payload.ptr() + size, sched.payload_len - size, offset);
				if(ret == -1) {
					throw exception_sys_t(log::error, errno, "pread(): %m");
				}

				size += ret;
				if(ret == 0 && size != sched.payload_len) {
					throw exception_log_t(log::error, "schedule payload out of range %ld %d", sched.payload_start, sched.payload_len);
				}
			}

			in_t::ptr_t start = payload;
			size_t limit = TAG_LEN;

			in_t::ptr_t tagstart = start;
			if(!tagstart.scan(" ", 1, limit))
				throw exception_log_t(log::error, "format error #0");
			tagstart++;

			in_t::ptr_t tagp = tagstart;

			if(!tagp.scan("\r\n", 2, limit))
				throw exception_log_t(log::error, "format error #1");
			tagp++; tagp++;

			tag = in_segment_t(tagstart, tagp - tagstart - 2);

			in_t::ptr_t end = start + (size_t)sched.payload_len;

			request = in_segment_t(tagp, end - tagp);

			if(interval_sleep > interval::zero) {
				if(bq_sleep(&interval_sleep) < 0) {
					return false;
				}
			}

			return true;
		} catch(...) {
			stop = true;
			throw;
		}
	}

	virtual void do_init() {
		MKCSTR(_ammo_filename, ammo_filename);
		MKCSTR(_schedule_filename, schedule_filename);

		ammo_fd = open(_ammo_filename, O_RDONLY, 0);
		if(ammo_fd < 0)
			throw exception_sys_t(log::error, errno, "open (%s): %m", _ammo_filename);

		schedule = fopen(_schedule_filename, "r");
		if(!schedule) {
			close(ammo_fd);
			throw exception_sys_t(log::error, errno, "fopen (%s): %m", _schedule_filename);
		}
	}

	virtual void do_run() const {}
	virtual void do_stat_print() const {}

	virtual void do_fini() {
		if(schedule) {
			fclose(schedule);
			schedule = NULL;
		}

		if(ammo_fd != -1) {
			close(ammo_fd);
			ammo_fd = -1;
		}
	}

	taurus_source_t(string_t const &name, config_t const &config) :
		source_t(name), ammo_fd(-1),
		ammo_filename(config.ammo), schedule_filename(config.schedule),
		deadline(timeval::current() + config.max_test_duration),
		loop_offset(-1), stop(false),
		timeval_last(timeval::current()) {}

private:
	mutex_t mutable mutex;
	int ammo_fd;
	FILE* schedule;
	string_t ammo_filename, schedule_filename;
	timeval_t deadline;

	mutable int64_t loop_offset;
	mutable std::atomic<bool> stop;
	mutable timeval_t timeval_last;
};

namespace source_log {
config_binding_sname(taurus_source_t);
config_binding_value(taurus_source_t, ammo);
config_binding_value(taurus_source_t, schedule);
config_binding_value(taurus_source_t, max_test_duration);
config_binding_cast(taurus_source_t, source_t);
config_binding_ctor(source_t, taurus_source_t);
}

}} // namespace phantom::taurus

#pragma GCC visibility pop
