# Kết quả bản giao diện chat

Môi trường Windows 11, Qt 6.12.0, MinGW 64-bit, Release, BUILD_TESTING=ON.

- Build thành công Server, ChatClient và các target kiểm thử.
- CTest: 7/7 bộ thành công, tổng 134,99 giây: transfer_tests, offline_tests, history_tests, connection_tests, auth_tests, password_tests, ui_tests.
- Sau chỉnh sửa bố cục cuối: chạy lại ui_tests, 7 mục PASS, 0 FAIL (gồm khởi tạo/kết thúc), 140 ms.
- UI kiểm tra giữ người nhận khi online thay đổi, về phòng chung khi người nhận offline, trạng thái điều khiển, bố cục 800×620 và 760×560, trạng thái truyền/hủy, chọn văn bản và escape nội dung, menu gọi đúng chức năng và Enter gửi.
- Ảnh Qt thật tại Extra/Screenshots, dùng dữ liệu minh họa cục bộ.
- Hash các .cpp/.h của Server và Shared không đổi so với bản đổi mật khẩu. Không đổi giao thức, đường dẫn dữ liệu hoặc schema SQLite.

Log: Logs/ui-ctest-all.txt, Logs/ui-final-tests.txt, Logs/ui-build-final.txt.
Kiểm thử offscreen có cảnh báo thư mục font của Qt; công cụ chụp ảnh tải font đã cài trên Windows, không phân phối font trong ZIP.

Chưa thử trực tiếp trên hai máy LAN với giao diện mới; bài đúng 1 GiB không chạy lại trong đợt này. Các bài hồi quy TLS/tài khoản/truyền dữ liệu nằm trong lần CTest nêu trên. Báo cáo phiên bản trước được giữ riêng, không coi là lần chạy lại bản UI.
