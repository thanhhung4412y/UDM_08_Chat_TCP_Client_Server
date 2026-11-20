# Kết quả bản tách cuộc trò chuyện

Build thành công Server, ChatClient và các target kiểm thử trên Windows 11 / Qt 6.12 MinGW 64-bit.

Kiểm thử bản này:
- history_tests: 20 PASS, 0 FAIL, gồm phân trang hơn 100 tin cho cùng người, room/Bob/Carol không lẫn nhau, mở lại database, chuyển cuộc trò chuyện trực tiếp và thông báo tin đến.
- ui_tests: 8 PASS, 0 FAIL, gồm số chưa đọc, xóa khi mở, giữ người đã offline và thêm người mới có tin riêng.
- Chạy CTest cho cả hai bộ: 2/2 thành công. Xem Logs/conversations-*.txt.

Server và Shared không thay đổi; không đổi schema SQLite, dữ liệu tài khoản hoặc giao thức. Bản này lọc bằng trường sender/to đã có trong packet lưu ở SQLite, phân trang giới hạn 100 tin; ảnh không bị nạp toàn bộ để lọc trong RAM. Chưa thêm index cho truy vấn hội thoại; database rất lớn có thể tải chậm.

Không chạy lại các bộ mạng/TLS hay bài đúng 1 GiB trong đợt này; chưa kiểm thử trực tiếp hai máy LAN. Các báo cáo UI_RESULTS/PASSWORD_RESULTS/... và log ui-* thuộc các phiên bản trước.

Bộ đếm chưa đọc nằm trong RAM của phiên hiện tại. Tin chỉ được lưu sau khi Client nhận; chưa có hộp thư offline. Reply vẫn hỗ trợ phòng chung như bản cũ.
