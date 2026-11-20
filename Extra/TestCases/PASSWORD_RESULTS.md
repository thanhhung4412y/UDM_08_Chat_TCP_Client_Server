# Kết quả đổi mật khẩu — 30/10/2026

## Kết quả bản mới

- Build Release thành công Client, Server và sáu bộ kiểm thử; Windows 11, Qt 6.12.0 MinGW 64-bit, GCC 13.1, CMake/Ninja.
- CTest: **6/6 bộ qua, 0 bộ lỗi**, **136,41 giây**: transfer_tests, offline_tests, history_tests, connection_tests, auth_tests, password_tests.
- Riêng password_tests trong lượt cuối: PASS, **39,04 giây**.
- Log cuối: **Logs/password-ctest-final.txt**. Log module chạy ban đầu (trước hai bài bổ sung cuối) được giữ dưới tên **password-module-first.txt**; không tính nhầm thành một lượt kiểm thử cuối khác.

## Các tình huống đã xác nhận

1. Mật khẩu hiện tại sai, mật khẩu mới quá ngắn hoặc trùng mật khẩu hiện tại: từ chối; mật khẩu cũ vẫn đăng nhập được trong các bài này.
2. Đổi đúng: mật khẩu cũ không đăng nhập được, mật khẩu mới đăng nhập được; đóng/mở lại database vẫn giữ kết quả.
3. Tên và UUID tài khoản không đổi. Một mục lịch sử SQLite đã lưu trước đó vẫn còn nguyên sau đổi mật khẩu.
4. TLS end-to-end: Client đang đăng nhập gửi yêu cầu, nhận thành công, chuyển Idle/đăng xuất và xóa thông tin mật khẩu cũ đang giữ để kết nối lại; đăng nhập cũ thất bại, đăng nhập mới thành công.
5. Gói yêu cầu cố chọn tên người khác bị bỏ qua trường tên: chỉ tài khoản của phiên xác thực được đổi. Tài khoản còn lại giữ mật khẩu cũ.
6. Yêu cầu trên TLS nhưng chưa đăng nhập bị từ chối, kể cả khi cung cấp đúng tên/mật khẩu cũ.
7. Hai yêu cầu đồng thời cùng dùng mật khẩu cũ: đúng một thành công. Lệnh UPDATE có điều kiện trên hash cũ không cho cả hai ghi đè lẫn nhau.
8. Snapshot xác thực cũ không còn hợp lệ sau cập nhật; snapshot mới hợp lệ. Server kiểm tra lại trước khi cấp phiên để tránh kết quả hash cũ đang chờ được dùng sau đổi mật khẩu.
9. Client mất kết nối ngay sau gửi: không báo thành công sai, không tự dùng lại thông tin cũ và không tự gửi lại yêu cầu. Phần xử lý worker vẫn báo kết quả đã commit cho Server khi cần thu hồi phiên.
10. Form che mật khẩu, kiểm tra xác nhận không khớp, xử lý khi chưa đăng nhập và mở lại nút sau lỗi.

Các bộ hồi quy còn lại xác nhận đăng ký/đăng nhập/TLS/pinning, chat chung/riêng, Reply/Forward, ảnh/sticker, truyền video/hủy/lỗi, SQLite và kết nối lại vẫn qua.

## Thay đổi kỹ thuật chính

- AccountStore.changePassword kiểm tra mật khẩu hiện tại, tạo salt/hash mới và UPDATE nguyên tử theo ID + hash cũ; không đổi schema, UUID hoặc tên tài khoản.
- AccountResult mang snapshot hash **chỉ nội bộ**; AuthService/Server đọc lại để chặn kết quả đăng nhập đã lỗi thời. Snapshot không được đưa vào gói JSON.
- AuthService dùng chung worker và giới hạn thử hiện có; kết quả đổi thành công vẫn được xử lý nếu socket yêu cầu vừa ngắt.
- Server lấy tài khoản từ phiên, dùng request_id đối chiếu phản hồi, thu hồi quyền phiên cũ trước khi yêu cầu đăng nhập lại.
- SessionConnection không tự gửi lại thao tác khi kết quả chưa xác định; xóa thông tin cũ giữ để retry nếu thành công hoặc mất kết nối khi đang đổi.
- PasswordDialog/main_client.cpp thêm nút và hộp nhập, chặn gửi yêu cầu khi đang truyền video, quay lại màn hình đăng nhập sau thành công.
- Các gói đổi mật khẩu không được lưu vào lịch sử. Không có mật khẩu mới trong cấu hình hoặc log.

## Giới hạn kiểm chứng

- TCP/TLS thật trên loopback một máy; chưa thử hai máy vật lý/router của bạn.
- Chưa tự động hóa toàn bộ thao tác main window native hoặc thử mất điện/đĩa hỏng đúng lúc ghi.
- Timeout 30 giây và trường hợp phản hồi mất có thông báo kết quả chưa xác định; bài mất kết nối đã chạy, không mô phỏng tất cả loại mất gói mạng.
- Bài video đúng 1 GiB qua TLS đã qua ở bản tài khoản trước (ACCOUNTS_RESULTS.md và log accounts-video-1GiB-TLS.txt). Không chạy lại bài 1 GiB ở lượt này vì mã truyền byte/TLS không đổi; các bài video thông thường đã chạy lại và qua.
- Một cảnh báo phông của nền tảng offscreen có thể xuất hiện khi chạy form test; các kiểm tra form đều qua.

## Kiểm tra bằng tay

1. Đóng Server/Client cũ, mở Code/CMakeLists.txt của bản này và Build. Chạy cả Server và Client mới; giữ nguyên thư mục dữ liệu.
2. Đăng nhập tài khoản đang có, gửi một tin nhắn và mở Lich su.
3. Bấm Doi mat khau, thử nhập sai mật khẩu cũ và xác nhận mới không khớp.
4. Nhập đúng, đợi thành công: Client quay lại màn hình đăng nhập, ô mật khẩu trống.
5. Thử mật khẩu cũ (thất bại), rồi mật khẩu mới (thành công). Kiểm tra lịch sử vẫn hiện như trước.
6. Đóng/mở lại Server và Client, xác nhận mật khẩu mới tiếp tục dùng được.

Không xóa accounts.sqlite hoặc đổi tên/IP/cổng nếu muốn tiếp tục đúng kho tài khoản/lịch sử cũ. Xem README.md cho tình huống mất kết nối khi đang đổi và cách sao lưu.
