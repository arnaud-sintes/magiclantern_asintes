platform=platform/5D3.
name=crop_rec_4k_mlv_snd_isogain_1x3_presets_ultrafast_focussq


# building 5D3 code with zip
function BuildCode
{
    cd ${platform}$1
    
    # rebuild if no second argument
    if [ -z "$2" ]
    then
        make clean
    fi

    make -j8 zip

    # back to root folder
    cd ../..
}


# build required modules
function BuildModules
{   
    cd modules

    # rebuild if no second argument
    if [ -z "$2" ]
    then
        make clean
    fi
    
    make

    # back to root folder
    cd ..   
}


# copy modules and LUA scripts in zip folder
function CopyToZip
{
    # copy modules to 5D3 zip folders
    find . -name \*.mo -exec cp {} ${platform}$1/zip/ML/modules/ \;

    # copy LUA scripts to 5D3 zip folders
    cp -r scripts/ ${platform}$1/zip/ML/
}


# package zip content with modules and LUA scripts
function Package
{
    # copy modules to 5D3 zip folders
    CopyToZip $1

    # zip file name
    file=$( date '+'${name}'%Y%b%d.5D3'$1'.zip' )

    # create 5D3 zip with date
    cd ${platform}$1/zip/
    zip -r $file .

    # move zip to root folder
    mv $file ../../../

    # back to root folder
    cd ../../..
}


# main excution
if [ -z "$1" ]
# no argument: full rebuild
then
    BuildCode "113"
    BuildCode "123"
    BuildModules
    rm *.zip
    Package "113"
    Package "123"
# argument: fast build
else
    BuildCode $1 fast
    BuildModules fast
    CopyToZip $1

    # use powershell outside WSL to copy zip content to SD card:
    # > ./copy5D3.ps1 "E:/"
fi