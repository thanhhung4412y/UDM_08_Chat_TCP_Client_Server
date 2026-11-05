# ChatTCP1 — Ứng dụng Chat TCP Client–Server (C++/Qt 6)

> Dự án được sắp xếp theo cấu trúc `ProjectStructure__Sample`. Những phần tài liệu chưa có được giữ bằng các `README.md` mẫu để nhóm tự bổ sung sau. Mã nguồn C++ và ảnh được giữ nguyên; chỉ cập nhật đường dẫn trong `Code/CMakeLists.txt`.

## Thành viên

| STT | MSSV | Họ và tên | Vai trò |
|---:|---|---|---|
| 1 | | | |
| 2 | | | |
| 3 | | | |
| 4 | | | |
| 5 | | | |
| 6 | | | |

## Giới thiệu

Ứng dụng nhắn tin TCP sử dụng C++ và Qt 6, gồm chương trình Server và Client có giao diện Qt Widgets.

## Kiến trúc hệ thống

- Mô hình: Client–Server.
- Giao thức: TCP, các gói JSON mỗi gói trên một dòng (`\n`).
- Port Server trong mã nguồn hiện tại: `12345`.
- Client trong mã nguồn hiện tại kết nối `127.0.0.1:12345` (máy cục bộ).

## Cấu trúc thư mục

```text
chattcp1/
├── README.md
├── .gitignore
├── WorkingWithGit__v0.1.pdf
├── Code/
│   ├── README.md
│   ├── CMakeLists.txt
│   ├── Server/               # main.cpp, chatserver.*, mainwindow.*
│   ├── Client/               # main_client.cpp, stickers.qrc, sticker/
│   └── Shared/               # Chưa có mã nguồn, để dành bổ sung
├── DOCX/                    # Báo cáo, tài liệu (bổ sung sau)
├── PPTX/                    # Slide (bổ sung sau)
└── Extra/                   # Minh chứng, test, sơ đồ, bản sao sticker
```

## Yêu cầu môi trường

- Ngôn ngữ: C++.
- Qt 6 với các mô-đun **Core**, **Widgets**, **Network**.
- CMake >= 3.19; trình biên dịch C++ tương thích Qt.
- Môi trường từng dùng trong dự án gốc: Qt Creator / Qt 6.12 MinGW 64-bit trên Windows.

## Cài đặt

1. Cài Qt 6 (gồm Qt Widgets và Network), Qt Creator và bộ công cụ biên dịch phù hợp.
2. Trong Qt Creator, chọn **Open Project** và mở `Code/CMakeLists.txt`.
3. Chọn Kit Qt 6 phù hợp, **Configure Project** rồi **Build** để CMake tạo build mới.

## Hướng dẫn chạy

### Server

- Trong Qt Creator, chọn target **`chattcp1`** và Run.
- Server trong `Code/Server/main.cpp` sử dụng cổng 12345.

### Client

- Chọn target **`ChatClient`** và Run.
- Có thể chạy nhiều Client sau khi Server khởi động; mỗi Client nhập tên người dùng.

## Cấu hình

- Thay đổi cổng Server tại `Code/Server/main.cpp` (`server.start(12345)`).
- Thay đổi IP/port tại `Code/Client/main_client.cpp` (`socket.connectToHost("127.0.0.1", 12345)`).
- Chức năng nhập IP/port trực tiếp trên giao diện cần bổ sung nếu nhóm yêu cầu kết nối qua mạng LAN.

## Chức năng

Các mục dưới đây là chức năng có đoạn mã xử lý trong dự án, **chưa được kiểm chứng bằng chạy thử trên môi trường mới**.

- [x] Kết nối Client–Server bằng TCP.
- [x] Chat nhóm / nhắn tin riêng, danh sách online.
- [x] Emoji, sticker và truyền ảnh JPEG.
- [x] Reply / Forward.
- [ ] Gửi video hoàn chỉnh (đang có phần chọn file và kiểm tra dung lượng).
- [ ] Lưu lịch sử chat qua các lần chạy.
- [ ] Nhập IP và port từ giao diện.

## Kiểm thử

- Functional test: *nhóm bổ sung sau*.
- Test dữ liệu không hợp lệ: *nhóm bổ sung sau*.
- Test mất kết nối: *nhóm bổ sung sau*.
- Stress test: *nhóm bổ sung sau*.
- Performance test: *nhóm bổ sung sau*.

Lưu ảnh chụp màn hình, kết quả test và sơ đồ tại `Extra/`.

## Demo

- Video: *bổ sung link sau*.
- Slide: `PPTX/` (*chưa có*).
- Báo cáo: `DOCX/` (*chưa có*).

## Giới hạn và lưu ý

- Chưa có dữ liệu để khẳng định việc biên dịch/chạy sau khi di chuyển thư mục; kiểm tra bằng Qt Creator trên Windows để xác nhận.
- Các thư mục `build/`, `build_backup/` và cấu hình máy cá nhân `.qtcreator/` trong ZIP gốc là kết quả build/cấu hình cục bộ, không được đưa vào bản tổ chức để nộp Git. File `chattcp1.zip` gốc không bị sửa.
- Thư mục `Extra/Backup_Original/` giữ bản sao sticker từ `stickers_backup/` ban đầu để tránh mất dữ liệu.
