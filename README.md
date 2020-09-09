# sac4hm

## Prerequisite

### Libs
You have to install the [`libzbc`](https://github.com/hgst/libzbc) library before compile this code. 
Once you had install the `libzbc`, check if the `libzbc*.so` is in the `/lib` dir.

### Traces
To execute this code normally, you need trace files as input requests, since the file size limitation in github, 
I put the needed trace files on Google Drive, you are demanded to download these files and manually put them into `traces/` dir of this repository. 

## Compilation and installation

To compile the library and all example applications under the tools directory, execute the following commands.

```
git clone https://github.com/dcstrange/sac4hm.git && cd sac4hm
make
```
That will generatet the executable file `test` at the root dir. 

## Test

one line tests: 
```
./test --algorithm CARS  --cache-size 16G 
```
```
./test --algorithm MOST  --cache-size 16G 
```
```
./test --algorithm CARS  --cache-size 16G --rmw-part 0
```
