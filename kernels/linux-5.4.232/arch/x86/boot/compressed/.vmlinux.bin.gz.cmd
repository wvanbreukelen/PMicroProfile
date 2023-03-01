cmd_arch/x86/boot/compressed/vmlinux.bin.gz := cat arch/x86/boot/compressed/vmlinux.bin arch/x86/boot/compressed/vmlinux.relocs | gzip -n -f -9 > arch/x86/boot/compressed/vmlinux.bin.gz
