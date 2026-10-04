# Signature fixture

`pyhanko-signed.pdf` is the public pyHanko test fixture
`minimal-signed-twice-both-created.pdf` from commit
`871387373d318882e81a3fdd5e8d9089b6f5170e`.

Source: https://github.com/MatthiasValvekens/pyHanko/blob/871387373d318882e81a3fdd5e8d9089b6f5170e/internal/common-test-utils/src/pyhanko_testing_commons/test_data/data/pdf/minimal-signed-twice-both-created.pdf

SHA-256: `2ef1b801c4d168d0deb5904f5d272e64e2f080c9273e92d8803272fd9d1721c9`

MIT license, copyright Matthias Valvekens; see `pyhanko-LICENSE`.
Only the signed PDF is included. No signing keys, PFX files or passwords are
included or needed. Tests use an empty trust store and ignore expiry for this
historical fixture, separately asserting cryptographic validity and untrusted
certificates. Tampering happens in memory on a covered PDF comment byte.
