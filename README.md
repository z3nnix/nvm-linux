# Overview
The literal heart of NovariaOS. Full behavioral compatibility (except for syscalls) with NVM in Novaria.

## Build from source
```
$ wget https://github.com/z3nnix/chorus/releases/download/1.0.1/chorus.zip \
 && unzip chorus.zip \
 && rm chorus.zip \
 && sudo mv chorus.bin /usr/bin/chorus \
 && sudo chmod +x /usr/bin/chorus \
 && chorus all
```

## Dependencies:
- GNU/Linux system
- Superuser rights
- Network access XD

## Supported systems:
- All distros(i hope) with Linux 4.4+ kernel

## Notes
- Bug reports are welcome
- [Docs available here](https://github.com/z3nnix/NovariaOS/blob/main/docs/2.1-Bytecode.md)