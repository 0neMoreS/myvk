clear
node .\Maekfile.js && ^
.\bin\main.exe ^
--physical-device "NVIDIA GeForce RTX 5080 Laptop GPU" ^
--drawing-size 1920 1080 ^
--camera "Camera" ^
--scene sphereflake-blue.s72 ^
--no-debug ^
--headless ^ < ./report/A1/headless_input/sphereflake.txt > ./drawer/headless_frame_time.txt