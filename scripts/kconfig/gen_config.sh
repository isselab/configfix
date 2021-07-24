#!/usr/bin/bash 
#
# Lets user select target architecture, sets ARCH variable,
# and then runs 'make randconfig' to generate a random .config 
# for that architecture.
# 
# This script expects $CONFIGFIX_TEST_PATH to be set.
# 
# The generated .config is saved into $CONFIG_TEST_PATH/$ARCH/config.XX
# together with generate.sh, which lets re-creating the same configuration
# by re-using the KCONFIG_SEED value, and run.sh, which allows starting
# xconfig with the generated .config.
# 

# check if $CONFIGFIX_TEST_PATH is set
if [ -z ${CONFIGFIX_TEST_PATH} ]; then
    echo "Error: CONFIGFIX_TEST_PATH is not set"
    exit
fi

# select architecture
echo "Select target architecture:"
select CHOICE in \
"Alpha" "ARC" "ARM 32-bit" "ARM 64-bit" "TI TMS320C6x" "C-SKY" \
"H8/300" "Hexagon" "IA-64" "Motorola 68000" "MicroBlaze" "MIPS" \
"NDS32" "Nios II" "OpenRISC" "PA-RISC" "PowerPC" "RISC-V" \
"IBM System/390 and z/Architecture" "SuperH 32-bit" "SuperH 64-bit" \
"SPARC 32-bit" "SPARC 64-bit" "User-Mode Linux" "x86-32" "x86-64" "Xtensa" \
; 
do 
  case $CHOICE in
    "Alpha")            export ARCH=alpha;      export SRCHARCH=alpha;      break;;
    "ARC")              export ARCH=arc;        export SRCHARCH=arc;        break;;
    "ARM 32-bit")       export ARCH=arm;        export SRCHARCH=arm;        break;;
    "ARM 64-bit")       export ARCH=arm64;      export SRCHARCH=arm64;      break;;
    "TI TMS320C6x")     export ARCH=c6x;        export SRCHARCH=c6x;        break;;
    "C-SKY")            export ARCH=csky;       export SRCHARCH=csky;       break;;
    "H8/300")           export ARCH=h8300;      export SRCHARCH=h8300;      break;;
    "Qualcomm Hexagon") export ARCH=hexagon;    export SRCHARCH=hexagon;    break;;
    "IA-64")            export ARCH=ia64;       export SRCHARCH=ia64;       break;;
    "Motorola 68000")   export ARCH=m68k;       export SRCHARCH=m68k;       break;;
    "MicroBlaze")       export ARCH=microblaze; export SRCHARCH=microblaze; break;;
    "MIPS")             export ARCH=mips;       export SRCHARCH=mips;       break;;
    "NDS32")            export ARCH=nds32;      export SRCHARCH=nds32;      break;;
    "Nios II")          export ARCH=nios2;      export SRCARCH=nios2;       break;;
    "OpenRISC")         export ARCH=openrisc;   export SRCARCH=openrisc;    break;;
    "PA-RISC")          export ARCH=parisc;     export SRCARCH=parisc;      break;;
    "PowerPC")          export ARCH=powerpc;    export SRCARCH=powerpc;     break;;
    "RISC-V")           export ARCH=riscv;      export SRCARCH=riscv;       break;;
    "IBM System/390 and z/Architecture") \
                        export ARCH=s390;       export SRCARCH=s390;        break;;
    "SuperH 32-bit")    export ARCH=sh;         export SRCARCH=sh;          break;;
    "SuperH 64-bit")    export ARCH=sh64;       export SRCARCH=sh;          break;;
    "SPARC 32-bit")     export ARCH=sparc32;    export SRCARCH=sparc;       break;;
    "SPARC 64-bit")     export ARCH=sparc64;    export SRCARCH=sparc;       break;;
    "User-Mode Linux")  export ARCH=um;         export SRCARCH=um;          break;;
    "x86-32")           export ARCH=i386;       export SRCARCH=x86;         break;;
    "x86-64")           export ARCH=x86_64;     export SRCARCH=x86;         break;;
    "Xtensa")           export ARCH=xtensa;     export SRCARCH=xtensa;      break;;
    * )                 echo "Invalid choice";
  esac
done
echo "
You selected $CHOICE. ARCH is set to '$ARCH'
"

# probabilities list
PROBABILITIES="10 20 30 40 50 60 70 80 90"

cd ../../

# generate base configuration, forward KCONFIG_SEED printout
# export KCONFIG_PROBABILITY=100
make randconfig 2> generate.sh

# move files files
mkdir -p $CONFIGFIX_TEST_PATH/$ARCH
mv .config $CONFIGFIX_TEST_PATH/$ARCH/.config.base
chmod +x generate.sh
mv generate.sh $CONFIGFIX_TEST_PATH/$ARCH

# for each probability
for PROBABILITY in $PROBABILITIES; do

  CONFIG_FILENAME=.config.$PROBABILITY

  # generate config sample, forward KCONFIG_SEED printout
  export KCONFIG_PROBABILITY=$PROBABILITY
  make randconfig 2> generate.sh
  
  # create directories if missing
  CONFIG_PATH=$CONFIGFIX_TEST_PATH/$ARCH/config.$PROBABILITY
  mkdir -p $CONFIG_PATH
  
  # move .config
  mv .config $CONFIG_PATH/$CONFIG_FILENAME
  
  # print generate.sh (may contain make and Kconfig warnings)
  cat generate.sh
  # move generate.sh
  chmod +x generate.sh
  mv generate.sh $CONFIG_PATH

  # create run.sh, substitute variable values for placeholders
  cp scripts/kconfig/run_template.sh run.sh
  sed -i "s/ ARCH=@/ ARCH=$ARCH/g" run.sh
  sed -i "s/ SRCARCH=@/ SRCARCH=$SRCARCH/g" run.sh
  sed -i "s/ @CONFIG_FILENAME@/ $CONFIG_FILENAME/g" run.sh
  sed -i "s/ CONFIGFIX_TEST_PROBABILITY=@/ CONFIGFIX_TEST_PROBABILITY=$PROBABILITY/g" run.sh

  # move run.sh
  chmod +x run.sh
  mv run.sh $CONFIG_PATH
done


# unset variables
unset ARCH
unset SRCARCH
unset CONFIG_PATH
unset KCONFIG_PROBABILITY
