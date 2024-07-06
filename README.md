# yolocam
RV1106 yolo based camera module

v4l2 thread----+--->RGA-------> NPU
               |
               +--->RGA-------> DRM


+-----------------------------------------------+
|
|               V4L2 THREAD
|
+------+---+------------------------------------+
       |   |
       |   |
       |   |
+------v---|------------------------------------+
|   [QUEUE]|
|          |   DISPLAY THREAD
|          |                          [QUEUE]
+----------|----------------------------^-------+
           |                            |
           |                            |
+----------v----------------------------+-------+
|        [QUEUE]
|              NPU THREAD
|
+-----------------------------------------------+



# TODO

- [x] Screen preview & detect results overlay

- [x] Serialport output

- [ ] Lan data publiser

- [ ] Online update/change AI model

- [ ] Web server for device config

- [ ] RTSP stream out

- [ ] UVC stream out
