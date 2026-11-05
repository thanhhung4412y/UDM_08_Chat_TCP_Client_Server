# Shared

- jsonframer.h: tách dòng JSON tăng dần, giới hạn 512 KiB mỗi khung.
- jsonchannel.h/.cpp: đọc/ghi QTcpSocket có giới hạn hàng đợi, nhường vòng lặp sự kiện.
- videoprotocol.h: dung lượng tối đa 1 GiB, khối 64 KiB, kiểm tra dữ liệu.
- videoclient.h/.cpp: truyền/nhận từng khối, QSaveFile, SHA-256, tín hiệu metadata sau khi hoàn tất.
- videorelay.h/.cpp: định tuyến và xác nhận theo tất cả người nhận, timeout, hủy.
- historyprotocol.h: loại tin cần lưu; UUID/thời gian thống nhất cho bản tin đã được Server chấp nhận.

SQLite là dữ liệu cục bộ ở Client; Server không lưu bản sao lịch sử hoặc video.

Ban tai khoan them authprotocol.h. JsonChannel chan gui JSON tren QSslSocket chua ma hoa va tinh hang doi ca plaintext/ciphertext. Ung dung dung TLS, khong ha xuong TCP thuan.
