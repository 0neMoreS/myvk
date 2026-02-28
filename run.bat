clear
node .\Maekfile.js && ^
.\bin\main.exe ^
--physical-device "NVIDIA GeForce RTX 5080 Laptop GPU" ^
--drawing-size 1920 1080 ^
--camera "Camera" ^
--scene ./report/A2/A2-materials-test.s72 ^
--no-debug ^
--exposure 0 ^
--tone-map aces 
@REM --headless < ./report/A2/headless_input/displacement_map.txt > ./drawer/headless_frame_time.txt