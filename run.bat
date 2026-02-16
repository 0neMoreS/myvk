clear
node .\Maekfile.js && ^
.\bin\main.exe ^
--physical-device "NVIDIA GeForce RTX 5080 Laptop GPU" ^
--drawing-size 1920 1080 ^
--camera "Camera" ^
--scene ./external/s72/examples/materials.s72 ^
--no-debug
@REM --headless < ./report/A1/headless_input/sphereflake.txt