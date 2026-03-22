clear
node .\Maekfile.js && ^
.\bin\main.exe ^
--physical-device "NVIDIA GeForce RTX 5080 Laptop GPU" ^
--drawing-size 1920 1080 ^
--scene ./external/s72/examples/lights-Spot-Shadows.s72 ^
--camera "Camera" ^
--exposure 0 ^
--reverse-z 
@REM --no-debug ^
@REM --headless < ./report/A3/headless_input/displacement_map.txt > ./scripts/headless_frame_time.txt
@REM --scene ./report/A3/A3-LightNumPBR.s72 ^

