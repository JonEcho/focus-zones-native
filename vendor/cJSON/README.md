# cJSON Vendor

This directory should contain `cJSON.c` and `cJSON.h` from https://github.com/DaveGamble/cJSON

To populate:
```
curl -o cJSON.h https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.h
curl -o cJSON.c https://raw.githubusercontent.com/DaveGamble/cJSON/master/cJSON.c
```

Or clone and copy:
```
git clone https://github.com/DaveGamble/cJSON.git
cp cJSON/cJSON.h cJSON/cJSON.c vendor/cJSON/
```
