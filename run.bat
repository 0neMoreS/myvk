clear
node .\Maekfile.js && ^
.\bin\main.exe ^
--physical-device "NVIDIA GeForce RTX 5080 Laptop GPU" ^
--drawing-size 1920 1080 ^
--camera "Camera-Shadow512" ^
--scene ./external/s72/examples/sg-Articulation.s72 ^
--exposure 0 ^
--no-debug
@REM --reverse-z ^
@REM --headless < ./report/A3/headless_input/displacement_map.txt > ./scripts/headless_frame_time.txt
@REM --scene ./report/A3/scene.s72 ^