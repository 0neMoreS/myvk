clear
node .\Maekfile.js && ^
.\bin\main.exe ^
--physical-device "NVIDIA GeForce RTX 5080 Laptop GPU" ^
--drawing-size 1920 1080 ^
--scene ./report/A3/A3-LightSort.s72 ^
--camera "Camera.001" ^
--exposure 0 ^
--reverse-z
@REM --headless < ./report/A2/headless_input/displacement_map.txt > ./drawer/headless_frame_time.txt
@REM --scene ./external/s72/examples/lights-Parameters.s72 ^
