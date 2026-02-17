clear
node .\Maekfile.js && ^
.\bin\main.exe ^
--physical-device "NVIDIA GeForce RTX 5080 Laptop GPU" ^
--drawing-size 1000 1080 ^
--camera "Camera" ^
--scene ./external/s72/examples/materials.s72 ^
--no-debug ^
--exposure 0 ^
--tone-map aces
@REM --headless < ./report/A1/headless_input/sphereflake.txt