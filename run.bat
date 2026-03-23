clear
node .\Maekfile.js && ^
.\bin\main.exe ^
--physical-device "NVIDIA GeForce RTX 5080 Laptop GPU" ^
--drawing-size 1920 1080 ^
--camera "Camera-Shadow512" ^
--scene ./report/A3/A3-Cascade.s72 ^
--exposure 0 ^
--reverse-z ^
--no-debug 
@REM --headless < ./report/A3/headless_input/displacement_map.txt > ./scripts/headless_frame_time.txt
@REM --scene ./external/s72/examples/lights-Spot-Shadows.s72 ^