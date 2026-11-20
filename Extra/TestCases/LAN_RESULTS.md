# Kết quả bản nhập IP/cổng — 29/10/2026

- Build Release thành công cả Client và Server, Qt 6.12 MinGW 64-bit trên Windows 11.
- CTest **4/4 bộ qua, 0 lỗi**, 42,29 giây: transfer_tests, offline_tests, history_tests, connection_tests.
- Bổ sung bài `hostnameConnection` và chạy riêng trên bản build cuối: PASS, đăng nhập qua `localhost` vào cổng Server tự chọn trong bài thử.
- Bộ kết nối kiểm tra: IPv4/IPv6/hostname, chuẩn hóa địa chỉ, port 1–65535, loại IP/URL sai; hộp thoại báo lỗi và nhớ cấu hình; tách lịch sử theo địa chỉ/cổng; TCP đăng nhập cổng khác; gọi kết nối khi đã Ready không đăng nhập lặp; mất kết nối rồi thử lại; Server chưa chạy rồi mở Server và thử lại; trùng tên rồi thử lại; Server không trả lời đăng nhập và hủy chờ.
- Bộ hồi quy vẫn qua: chat, tin riêng, Reply/Forward, ảnh/sticker, truyền video, hủy/timeout, SHA-256, lưu và đọc lại SQLite qua TCP.
- Video 1 GiB qua TCP + SQLite đã được kiểm chứng ở bản SQLite trước (log và báo cáo **SQLITE_RESULTS.md** được giữ lại). Không chạy lại bài 1 GiB trong lượt này vì mã chia khối/ghi video/SQLite không đổi. Các bài video thông thường đã chạy lại trên bản mới.
- Chưa thử hai máy vật lý trong cùng LAN hoặc cài đặt tường lửa/router. Chưa tự động hóa toàn bộ thao tác đổi Server trong cửa sổ chat; nên thử thủ công theo **../../HUONG_DAN_LAN.md** trước khi demo.
- Cấu hình/lịch sử cá nhân và thư mục build không được đưa vào ZIP.

## File thay đổi chính

- Client/main_client.cpp: đọc cấu hình từ hộp thoại, trạng thái kết nối, thử lại và đổi phiên; không dùng IP/cổng cố định.
- Client/connectionprofile.*: chuẩn hóa/kiểm tra tên, địa chỉ, cổng; nhớ cấu hình QSettings.
- Client/connectiondialog.*: hộp nhập thông tin.
- Client/sessionconnection.*: TCP + đăng ký tên, timeout 10 giây, xử lý lỗi và thử lại.
- Server/main.cpp: chọn cổng lúc mở, thử cổng khác nếu bận, hiển thị IP mạng.
- CMakeLists.txt: thêm thư viện kết nối và bộ kiểm thử.

Log mới có tiền tố **lan-** trong **Logs/**. Log không tiền tố thuộc bản SQLite trước, giữ làm bằng chứng hồi quy của phần không đổi.
