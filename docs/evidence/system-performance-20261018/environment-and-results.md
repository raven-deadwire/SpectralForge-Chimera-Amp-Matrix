# Performance environment and observations

No certified CPU/RAM minimum or instance capacity is established by this table.

| Environment field | Observed / declared value |
| --- | --- |
| id | observed-linux |
| os_family | Linux |
| os_version | Linux-6.18.55-x86_64-with-glibc2.39 |
| architecture | x86_64 |
| cpu_model | INTEL(R) XEON(R) PLATINUM 8573C |
| physical_cores | UNKNOWN |
| logical_cores | 9 |
| allocated_logical_cores | 9 |
| ram_bytes | 10451435520 |
| virtualization | container |
| resource_constraints | ['cgroup cpu.max=800000 100000', 'cgroup memory.max=8589934592', 'cgroup cpuset.cpus.effective=0-8', 'No /dev/snd audio device exposed'] |
| daw_name | UNKNOWN |
| daw_version | UNKNOWN |
| plugin_format | UNKNOWN |
| interface_model | UNKNOWN |
| interface_firmware | UNKNOWN |
| driver_name | UNKNOWN |
| driver_version | UNKNOWN |
| power_mode | UNKNOWN |
| power_source | UNKNOWN |
| scheduler | UNKNOWN |
| notes | ['Auto-detected host facts; fill host/device/power fields before measurement.', 'Host-visible RAM/cores are not necessarily the container allocation.'] |

| Run | Status | wall p99 / peak (µs) | thread CPU (%) | compute over budget / scheduled miss | sampled RSS peak (bytes) | soak |
| --- | --- | ---: | ---: | --- | ---: | --- |
| core-classic-none-i1-b128-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-classic-none-i1-b128-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-classic-none-i1-b256-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-classic-none-i1-b256-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-classic-none-i2-b128-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-classic-none-i2-b128-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-classic-none-i2-b256-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-classic-none-i2-b256-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-classic-none-i4-b128-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-classic-none-i4-b128-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-classic-none-i4-b256-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-classic-none-i4-b256-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-dual-Blend-i1-b128-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-dual-Blend-i1-b128-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-dual-Blend-i1-b256-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-dual-Blend-i1-b256-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-dual-Blend-i2-b128-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-dual-Blend-i2-b128-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-dual-Blend-i2-b256-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-dual-Blend-i2-b256-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-dual-Blend-i4-b128-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-dual-Blend-i4-b128-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-dual-Blend-i4-b256-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-dual-Blend-i4-b256-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-matrix-none-i1-b128-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-matrix-none-i1-b128-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-matrix-none-i1-b256-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-matrix-none-i1-b256-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-matrix-none-i2-b128-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-matrix-none-i2-b128-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-matrix-none-i2-b256-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-matrix-none-i2-b256-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-matrix-none-i4-b128-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-matrix-none-i4-b128-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-matrix-none-i4-b256-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| core-matrix-none-i4-b256-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| dual_crossover-dual-Crossover-i1-b128-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| dual_crossover-dual-Crossover-i1-b128-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| dual_crossover-dual-Crossover-i1-b256-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| dual_crossover-dual-Crossover-i1-b256-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| dual_crossover-dual-Crossover-i2-b128-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| dual_crossover-dual-Crossover-i2-b128-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| dual_crossover-dual-Crossover-i2-b256-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| dual_crossover-dual-Crossover-i2-b256-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| dual_crossover-dual-Crossover-i4-b128-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| dual_crossover-dual-Crossover-i4-b128-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| dual_crossover-dual-Crossover-i4-b256-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| dual_crossover-dual-Crossover-i4-b256-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| gui_focus-classic-none-i4-b128-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| gui_focus-classic-none-i4-b128-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| gui_focus-classic-none-i4-b256-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| gui_focus-classic-none-i4-b256-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| gui_focus-dual-Blend-i4-b128-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| gui_focus-dual-Blend-i4-b128-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| gui_focus-dual-Blend-i4-b256-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| gui_focus-dual-Blend-i4-b256-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| gui_focus-matrix-none-i4-b128-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| gui_focus-matrix-none-i4-b128-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| gui_focus-matrix-none-i4-b256-t0-playback | BLOCKED | — | — | — | — | BLOCKED |
| gui_focus-matrix-none-i4-b256-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| soak-matrix-none-i4-b128-t-2-playback | BLOCKED | — | — | — | — | BLOCKED |
| soak-matrix-none-i4-b128-t-2-recording | BLOCKED | — | — | — | — | BLOCKED |

## Reasons

- BLOCKED: No integrated Release product binary, DAW, host callback adapter or audio device available in this executor; /proc PID namespace is unsuitable for own-process RSS sampling
