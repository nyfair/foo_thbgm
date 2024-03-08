find -name Debug | xargs rm -rf
find -name Release | xargs rm -rf
find -name x64 | xargs rm -rf
rm -rf .vs *.sdf
