# Kết quả bản tìm kiếm tin nhắn

Windows 11 / Qt 6.12.0 / MinGW 64-bit / Release / BUILD_TESTING=ON.

- Build thành công Server, ChatClient, tất cả target kiểm thử và công cụ xem trước.
- CTest cuối: 3/3 bộ liên quan qua, 0 lỗi: history_tests (4,28s), ui_tests (0,31s), search_tests (0,80s), tổng 5,41 giây.
- search_tests chạy riêng trước lượt cuối: 7 mục PASS gồm init/cleanup, 0 FAIL, 662 ms.
- Kiểm tra có/không dấu tiếng Việt, Unicode tổ hợp, chữ hoa/thường, dấu nháy/SQL literal, % và _, tìm nội dung/quote/tên video, không lẫn room/Bob/Carol.
- 135 tin riêng xen kẽ tin chung: phân trang 50/50/35, không lặp/bỏ tin; context đúng 21 tin cùng hội thoại; từ chối sequence thuộc phòng khác.
- Sau mở lại bằng SQLite chỉ đọc vẫn tìm được lịch sử cũ. Kết quả không có BLOB ảnh; context nạp lại ảnh đúng byte. Kết nối chỉ đọc không ghi được; thiếu database không tạo tệp mới.
- UI: tô vàng theo sequence, nội dung HTML vẫn là văn bản, chuyển trang, chọn kết quả, đổi từ khóa nhanh bỏ kết quả cũ, xóa từ khóa, hủy khi đóng, báo lỗi database thiếu.
- Kiểm tra ảnh Qt thật: tìm `da nang` thấy nội dung và câu Reply có `Đà Nẵng`. Ảnh trong Extra/Screenshots dùng dữ liệu giả và SQLite tạm, không truy cập tài khoản thật.

Hash .cpp/.h của Server và Shared giữ nguyên so với bản Profile. Không đổi schema, dữ liệu tài khoản/lịch sử hoặc giao thức. Chỉ thêm đọc tìm kiếm và UI Client. Có tổng 9 bộ kiểm thử trong CMake; đợt này chạy lại 3 bộ có liên quan, không tuyên bố đã chạy lại các bộ mạng/tài khoản/video hoặc bài đúng 1 GiB. Báo cáo PROFILE_RESULTS.md và các báo cáo/log cũ là kết quả phiên bản trước.

Tìm nền bằng QThreadPool một worker, mỗi tác vụ có kết nối SQLite riêng chỉ đọc. Quét tuần tự metadata và lọc chuỗi đã chuẩn hóa; bộ nhớ kết quả tối đa 50 tin. Có hủy và số phiên tác vụ để bỏ phản hồi lỗi thời. Không có chỉ mục tìm kiếm riêng nên lịch sử rất lớn có thể chậm. Context tối đa 21 tin có thể nạp ảnh, vẫn giữ giới hạn renderer/video link hiện có.

Chưa thử trực tiếp hai máy LAN trong đợt này. Reply cũ chỉ lưu câu trích dẫn, chưa có ID tin gốc để nhảy chính xác ra ngoài đoạn context. Chỉ tìm dữ liệu đã lưu cục bộ, không OCR ảnh hoặc tìm trong nội dung video. Cảnh báo thư mục font Qt khi offscreen không làm lỗi kiểm thử; công cụ chụp ảnh dùng font hệ thống, không kèm font trong ZIP.

Log: Logs/search-ctest.txt, search-ctest-detail.txt, search-tests.txt, search-build*.txt.
