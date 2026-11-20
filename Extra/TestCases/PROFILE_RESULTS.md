# Kết quả bản hiện mật khẩu, khôi phục và ảnh đại diện

Môi trường: Windows 11, Qt 6.12.0, MinGW 64-bit, Release, BUILD_TESTING=ON.

- Build thành công Server, ChatClient, công cụ xem trước và các target kiểm thử.
- CTest bản cuối: 8/8 bộ qua, 0 bộ lỗi, tổng 259,79 giây: transfer_tests (62,61s), offline_tests (2,50s), history_tests (4,40s), connection_tests (20,34s), auth_tests (47,51s), password_tests (66,14s), ui_tests (0,36s), profile_tests (55,85s).
- profile_tests cũng chạy riêng: 8 mục PASS gồm init/cleanup, 0 FAIL. Kiểm tra quay vòng mã, mã sai, sai tên, mật khẩu mới ngắn, tiêu thụ mã, mở lại database, giữ UUID/ảnh, mã cũ vô hiệu sau đổi mật khẩu, hai yêu cầu reset đồng thời chỉ một thành công.
- UI: mặc định che nội dung, nút mắt hiện/ẩn giữ nguyên văn bản, mode quên mật khẩu hiện ô mã/nhập lại, cả ba ô đổi mật khẩu có nút mắt.
- TLS: tạo mã cần mật khẩu đúng, đặt lại thành công thu hồi phiên đang online, đăng nhập bằng mật khẩu mới; ảnh không hợp lệ bị từ chối, tên giả trong yêu cầu không đổi được chủ ảnh, ảnh đúng lưu theo phiên và được gửi lại khi đăng nhập lại.
- Kiểm tra JPEG hỏng, base64 sai, quá 16 KiB, chiều lớn hơn 128; UI nhận/xóa ảnh đại diện.
- Chụp và kiểm tra trực quan màn hình login/reset cùng bố cục chat 1120×760 / 800×620. Ảnh dùng dữ liệu giả, không có tài khoản hay ảnh cá nhân thực.

Log: Logs/profile-ctest.txt, profile-ctest-detail.txt, profile-tests.txt, profile-build*.txt.

Hai bảng phụ recovery và avatars được tạo nếu chưa có; không xóa bảng accounts hoặc thay schema lịch sử. Mã được băm SHA-256, gắn với hash mật khẩu lúc tạo; reset cập nhật mật khẩu và xóa mã trong một transaction. Bộ giới hạn yêu cầu/worker tài khoản áp dụng cả khôi phục. Video/chunk protocol không thay đổi.

Giới hạn: chưa thử trực tiếp hai máy LAN trong đợt này; bài đúng 1 GiB mặc định bỏ qua và không chạy lại. Mã phải được tạo trước khi quên, không có email/OTP hoặc reset quản trị. Bộ đếm chưa đọc chỉ theo phiên; hồ sơ người offline chưa đồng bộ từ Server. Các báo cáo/log có tiền tố phiên bản trước là kết quả lịch sử, không phải lần chạy mới.

Kiểm thử offscreen có cảnh báo thư mục font Qt và cảnh báo JPEG từ ca thử ảnh hỏng; không phải lỗi kiểm thử. Công cụ chụp ảnh dùng font hệ thống Windows, không phân phối font trong ZIP.
