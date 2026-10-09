# Experiment statistics

### KEM micro-benchmark — `kem_pc_x86_64.csv`

| metric | n | median (ms) | p95 (ms) | p99 (ms) | max (ms) |
|---|---|---|---|---|---|
| keygen | 1000 | 0.023 | 0.034 | 0.056 | 0.116 |
| encapsulation | 1000 | 0.013 | 0.015 | 0.020 | 0.067 |
| decapsulation | 1000 | 0.020 | 0.021 | 0.032 | 0.070 |

Cycles: 1000; secret mismatches: 0.

### KEM micro-benchmark — `kem_opi_armhf.csv`

| metric | n | median (ms) | p95 (ms) | p99 (ms) | max (ms) |
|---|---|---|---|---|---|
| keygen | 1000 | 0.700 | 0.725 | 0.745 | 1.796 |
| encapsulation | 1000 | 0.404 | 0.424 | 0.431 | 0.533 |
| decapsulation | 1000 | 0.621 | 0.642 | 0.652 | 0.728 |

Cycles: 1000; secret mismatches: 0.

### Network — `net_pc_baseline.csv`

| metric | n | median (ms) | p95 (ms) | p99 (ms) | max (ms) |
|---|---|---|---|---|---|
| tcp_connect | 1000 | 1.377 | 1.752 | 1.993 | 5.877 |
| tls_handshake | 1000 | 8.705 | 9.579 | 9.986 | 10.176 |
| key_confirm | 1000 | 9.283 | 10.133 | 10.564 | 11.830 |
| cpu | 1000 | 0.714 | 1.443 | 1.751 | 2.120 |

Negotiated: group=`x25519` version=`TLSv1.3` cipher=`TLS_AES_256_GCM_SHA384`; resumed(any measured)=False.
Handshake bytes (median): in=1283.0 out=1260.0 (TLS-record level, BIO counters).
Peak process RSS (whole process): 6144 KB.
Connections: ok=1000, errors=0, timeouts=0.

### Network — `net_pc_pq.csv`

| metric | n | median (ms) | p95 (ms) | p99 (ms) | max (ms) |
|---|---|---|---|---|---|
| tcp_connect | 1000 | 1.381 | 1.797 | 2.000 | 5.165 |
| tls_handshake | 1000 | 9.944 | 10.823 | 11.632 | 14.642 |
| key_confirm | 1000 | 9.246 | 10.079 | 11.283 | 12.869 |
| cpu | 1000 | 0.792 | 1.487 | 1.934 | 2.272 |

Negotiated: group=`X25519MLKEM768` version=`TLSv1.3` cipher=`TLS_AES_256_GCM_SHA384`; resumed(any measured)=False.
Handshake bytes (median): in=2371.0 out=2436.0 (TLS-record level, BIO counters).
Peak process RSS (whole process): 6144 KB.
Connections: ok=1000, errors=0, timeouts=0.

### Network — `net_opi_baseline.csv`

| metric | n | median (ms) | p95 (ms) | p99 (ms) | max (ms) |
|---|---|---|---|---|---|
| tcp_connect | 1000 | 0.000 | 0.000 | 0.000 | 0.000 |
| tls_handshake | 1000 | 14.624 | 15.638 | 16.017 | 16.531 |
| key_confirm | 1000 | 1.904 | 2.349 | 2.521 | 3.035 |
| cpu | 1000 | 14.203 | 14.387 | 14.500 | 14.803 |

Negotiated: group=`x25519` version=`TLSv1.3` cipher=`TLS_AES_256_GCM_SHA384`; resumed(any measured)=False.
Handshake bytes (median): in=1260.0 out=1283.0 (TLS-record level, BIO counters).
Peak process RSS (whole process): 5080 KB.
Connections: ok=1000, errors=0, timeouts=0.

### Network — `net_opi_pq.csv`

| metric | n | median (ms) | p95 (ms) | p99 (ms) | max (ms) |
|---|---|---|---|---|---|
| tcp_connect | 1000 | 0.000 | 0.000 | 0.000 | 0.000 |
| tls_handshake | 1000 | 15.860 | 16.897 | 17.632 | 20.733 |
| key_confirm | 1000 | 1.829 | 2.257 | 2.818 | 5.503 |
| cpu | 1000 | 15.394 | 15.608 | 15.811 | 16.036 |

Negotiated: group=`X25519MLKEM768` version=`TLSv1.3` cipher=`TLS_AES_256_GCM_SHA384`; resumed(any measured)=False.
Handshake bytes (median): in=2436.0 out=2371.0 (TLS-record level, BIO counters).
Peak process RSS (whole process): 5080 KB.
Connections: ok=1000, errors=0, timeouts=0.
