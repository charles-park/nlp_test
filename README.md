# Label Printer Test(Network Printer)
Label Printer with ODROID-C4 Printer server Test (GC420d/ZD230d)

* ODROID-C4를 Printer Server로 활용하여 Network으로 프린터 Data를 보내 출력하는 테스트 app. 
* Network TCP/IP Port 8888을 사용함. 
* 적용모델 : Zebra GC420D (단종 2022년 초), ZD230D 
* 프린터 프로토콜 : EPL Protocol 

2022년 4월 20일
version 명령어 추가 :
  version명령어를 보내서 응답이 있는 경우 새로운 문자열방식으로 전송함.
  응답이 없는 경우 기존 프로그램이므로 기존 

sudo apt install cups cups-bsd python3-pip
sudo usermod -aG lpadmin odroid

python3 -m pip install zebra cups psutil

#check printer
lpinfo -v

lpstat -v
lpadmin -p zebra -E -v usb://Zebra%20Technologies/ZTC%20GC420d%20\(EPL\)
lpstat -v


### Make ODROID-C4 Printer server
사용이미지 : project/n2l/buntu-22.04-4.9-minimal-odroid-c4-hc4-20220705.img
1. apt update && apt upgrade
2. apt install build-essential git overlayroot vim ssh cups cups-bsd python3-pip python3
3. python3 -m pip install zebra cups psutil
4. lpinfo -v
5. lpstat -v
6. lpadmin -p zebra -E -v usb://Zebra%20Technologies/ZTC%20GC420d%20\(EPL\)
7. lpstat -v
8. git clone https://github.com/charles-park/nlp_test
9. cd nlp_test/iperf_label_printer/install
10. service install : n2l-server/service/install.sh
11. overlay enable
```
root@odroid:~# update-initramfs -c -k $(uname -r)
update-initramfs: Generating /boot/initrd.img-4.9.277-75
root@odroid:~#
root@odroid:~# mkimage -A arm64 -O linux -T ramdisk -C none -a 0 -e 0 -n uInitrd -d /boot/initrd.img-$(uname -r) /media/boot/uInitrd 
Image Name:   uInitrd
Created:      Wed Feb 23 09:31:01 2022
Image Type:   AArch64 Linux RAMDisk Image (uncompressed)
Data Size:    13210577 Bytes = 12900.95 KiB = 12.60 MiB
Load Address: 00000000
Entry Point:  00000000
root@odroid:~#

overlayroot.conf 파일의 overlayroot=””를 overlayroot=”tmpfs”로 변경합니다.
vi /etc/overlayroot.conf
overlayroot_cfgdisk="disabled"
overlayroot="tmpfs"
```
12. overlay modified/disable  
```
[get write permission]
root@odroid:~$ sudo overlayroot-chroot 
INFO: Chrooting into [/media/root-ro]
root@odroid:/# 

[disable overlayroot]
overlayroot.conf 파일의 overlayroot=”tmpfs”를 overlayroot=””로 변경합니다.
vi /etc/overlayroot.conf
overlayroot_cfgdisk="disabled"
overlayroot=""

```
