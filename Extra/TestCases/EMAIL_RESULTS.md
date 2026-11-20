# Email verification / Gmail recovery — validation report

Environment: Windows 11, Qt 6.12.0, MinGW, C++17, Release build, Qt Widgets offscreen. Tests use isolated workspace SQLite/settings/TLS directories. No real Gmail account or production data is included.

## Functional validation

- Required registration email is validated at both ends; SMTP/header injection and malformed addresses rejected.
- Account creation and unique verified email association are committed together. A duplicate email rolls back account creation.
- Existing accounts may attach their first email only with the current account password plus proof of inbox ownership. Existing verified addresses cannot be replaced by this flow.
- Eight-digit challenges expire in ten minutes, bind to socket/name/email/purpose, allow at most five attempts, and are consumed before database work. Wrong socket, expired/replayed challenge and rate limits are tested.
- Recovery requires the account's already verified email. The credential snapshot prevents a pending reset from overwriting a password that has changed meanwhile. Old backup codes are removed and authenticated sessions revoked after reset.
- The Qt session controller is tested for email registration and reset, including returning to Idle without duplicate success dialogs. Legacy direct register packets cannot create/log into accounts.
- Local SMTP fixture tests the actual Gmail transport code: certificate-verified implicit TLS, multiline SMTP reply parsing, AUTH LOGIN, envelope, base64 body, success only after DATA acceptance, authentication rejection, and rejection of an untrusted certificate before credentials are sent.
- TLS chat-server integration uses an injected local mailbox to exercise registration, binding an old account, unknown-address recovery, password reset and revocation. These tests do not contact Gmail.
- PowerShell launcher parsed successfully (zero syntax errors). No administrator credentials were supplied, no live Gmail message was sent, and the launcher was not used to sign into a real Gmail account.
- Native UI previews inspected for registration, email recovery and legacy account linking; screenshots are synthetic. All password fields retain the visibility toggle.

## Regression scope and corrections

The initial full regression run passed 12/13 suites. The connection timeout test found that a new hardcoded connection timeout overrode the constructor setting; the final source restores the configured timeout. The targeted final connection run passed 26 tests; the extended email run passed 8 entries including setup/cleanup. The final full-suite log is included separately.

Non-email test fixtures now create accounts directly in isolated SQLite databases and log in normally. They do not enable a production verification bypass. Email registration itself is covered by both the session-controller and full server tests above.

## Practical limits

Gmail delivery to a real inbox, Google account policy/App Password acceptance, firewall access to smtp.gmail.com:465, and real two-machine foreground UI flows still require manual deployment verification. A successful SMTP submission does not guarantee inbox delivery. Unknown recovery addresses and failed delivery receive the same generic client response; this is not a constant-time protocol.

The regular transfer regression runs; the opt-in exact 1 GiB stress test was not rerun for this email change. Historical test reports retained in this folder describe earlier versions.

## Reproduce

Build with `-DBUILD_TESTING=ON` using the Qt/MinGW kit. Run `ctest --test-dir <build> --output-on-failure` with Qt DLLs on PATH. `email_tests -o email-results.txt,txt` produces a detailed report. Qt offscreen mode is configured on GUI test targets. No Gmail environment variables are needed for tests.

Final full regression: **13/13 suites passed, 0 failed, 135.67 seconds**. Final binaries include the SMTP transport and session-controller fixes. `Logs/email-ctest-final.txt` is the complete CTest summary; `Logs/email-final-detail.txt` and `Logs/email-connection-detail.txt` include the targeted detailed results.
