<div align="center">

<h1>pciemu</i></h1>
 
</div>

Del proyecto original:
> pciemu provides an example of PCIe Device Emulation in QEMU.
>
> The idea is to help those willing to explore PCIe devices but do not have access
to a real hardware. Or maybe someone with research ideas for a new PCIe device
or capability who want to easily test those ideas.
>
> Virtual devices can also be used to speed up the development process of a new
PCIe device and create test suites that do not require the existence of the
physical HW.
>
> Please note that pciemu implements a relatively simple device, with the goal
mainly being to provide an initial tutorial on how to write a virtual PCIe device
in QEMU. 

## Diferencias con pciemu original
Me he asegurado de que funciona con QEMU stable-9.1, especialmente con la
arquitectura RISC-V.

## Configuración y ejecución
1. Obtener una imagen de Linux para RISC-V de cualquier distribución. En mi caso usado una *build* automática de Debian que he obtenido de https://people.debian.org/~gio/dqib (concretamente la "riscv64-virt").
2. Clonar el repositorio, compilar e instalar QEMU (a no ser que ya dispongas de la versión 9.1 con soporte para RISC-V):
```sh
git clone https://github.com/Dorovich/pciemu.git
cd pciemu
```
Para compilar QEMU necesitarás sus dependencias. En Debian y derivados se puede usar el comando siguiente:
```sh
sudo apt update
sudo apt install -y git libglib2.0-dev libfdt-dev libpixman-1-dev zlib1g-dev \
     	 	    ninja-build netcat libssh-dev libvde-dev libvdeplug-dev \
		    libcap-ng-dev libattr1-dev libslirp-dev
```
Una vez tenemos las dependencias podemos compilar QEMU:
```
./setup.sh
cd qemu
make -j $(nproc --ignore 2)
sudo make install
```
3. Obtener OpenSBI y U-Boot (si no se tienen ya). En sistemas derivados de debian o ubuntu se pueden obtener de los repositorios oficiales:
```sh
sudo apt install opensbi u-boot-qemu
```
4. Arrancar la máquina virtual e iniciar sesión (usuario y contraseña por defecto son "debian" (debian) y "root" (root)). Puedes usar un comando simiar al siguiente:
```sh
qemu-system-riscv64 \
    -machine 'virt' \
    -cpu 'rv64' \
    -m 1G \
    -smp 4 \
    -device virtio-blk-device,drive=hd \
    -drive file=image.qcow2,if=none,id=hd \
    -device virtio-net-device,netdev=net \
    -netdev user,id=net,hostfwd=tcp::2222-:22 \
    -bios /usr/lib/riscv64-linux-gnu/opensbi/generic/fw_jump.elf \
    -kernel /usr/lib/u-boot/qemu-riscv64_smode/uboot.elf \
    -object rng-random,filename=/dev/urandom,id=rng \
    -device virtio-rng-device,rng=rng \
    -nographic \
    -append "root=LABEL=rootfs console=ttyS0" \
    -device pciemu,id=pciemu1 \
    -name "Debian-virt (rv64)"
```
5. Instalar paquetes necesarios para compilar el driver y el programa de usuario:
```sh
sudo apt install git make gcc linux-headers-$(uname -r)
```
6. clonar el repositorio en la máquina virtual, compilar y cargar el driver del dispositivo:
```sh
mkdir src
git clone https://github.com/Dorovich/pciemu.git
cd pciemu/src/sw/kernel
make
sudo insmod pciemu.ko
cd ../userspace
make
./pciemu_example -h
```