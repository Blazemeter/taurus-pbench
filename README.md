# BlazeMeter Enhancements for Phantom-Benchmark
Taurus modules for phantom-benchmark, high-RPS load generator

```
source_t taurus_source = taurus_source_t {
    ammo = "payload.src"  # payload file
    schedule = "schedule.sch"  # schedule file
    max_test_duration = 10s  # hard limit for test duration
}
```

## Payload File Format
```
<len> <marker>\r\n
<request>\r\n
```

`\r\n` can be replaced with `\n`

## Schedule File Format

Binary file with 16-byte records of following struct:

```
struct Schedule {
    enum Type {
	SCHEDULE = 0,
	START_LOOP,
	STOP
    };

    uint32_t type_and_delay_time_ms; // type in highest byte
    uint32_t payload_len;
    uint64_t payload_start;
};
```

- delay time means delay from previous record, special value `0xFFFFFF` means "no delay"
- payload start is offset in payload file to meta-line start
- payload len contains both meta-line and payload, but no last \r\n
- schedule file can be appended at any time and the tool will read it
