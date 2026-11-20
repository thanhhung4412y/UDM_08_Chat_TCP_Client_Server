# Password minimum 8 + email delivery diagnostics

Latest update validation: **4/4 relevant suites passed; 0 failed; 51.10 seconds**. Suites: password_tests, email_tests, auth_tests, connection_tests. Both Server and Client built successfully with Qt 6.12.0 / MinGW Release.

New boundary coverage rejects 7 characters, accepts exactly 8 and 128, rejects 129 and NUL. The account-store test exercises registration/login, password change, backup-code reset, email reset and reopening SQLite with 8-character credentials. The actual TLS session email controller registers with an 8-character password and resets to another 8-character password. Existing longer-password tests still pass. PBKDF2 hashing settings and existing account data are unchanged.

New email failure coverage injects failed SMTP for both registration and legacy-account binding, verifies a client error without challenge_id, and checks administrator status emission. Unknown-account recovery still returns its generic challenge response. The actual SMTP transport fixture continues to cover verified TLS, successful DATA acceptance, auth rejection and untrusted-certificate rejection. Diagnostic strings identify fixed stages only; they do not expose SMTP replies, addresses, passwords or OTPs.

No live Gmail credentials were used, no real email was sent by these tests, and the cause of the user's missing live email remains unconfirmed. The new Gmail status on the Server window is intended to diagnose that deployment. SMTP DATA acceptance does not guarantee inbox placement. Gmail App Password length remains 16; only chat-account minimum becomes 8.

A previous initial-email version passed 13/13 full regression suites; that historical result is in EMAIL_RESULTS.md. For this small authentication/diagnostic update, the four relevant suites above were rerun. Exact 1 GiB stress and physical two-machine LAN were not rerun.

Logs: Logs/password8-mail-build.txt, Logs/password8-mail-tests.txt. Synthetic UI screenshots show the updated 8-character hints.
