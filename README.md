## Install 
### 1. Install libpq
For Mac 
```bash
brew install libpq
```
For Ubuntu
```bash
sudo apt-get install libpq-dev
```
### 2. Config
```bash
$ pg_config --includedir
/usr/local/include
```

If you have pkg-config installed, you can run instead:
```bash
$ pkg-config --cflags libpq
-I/usr/local/include
```

You can find out the library directory using pg_config as well:
```bash
$ pg_config --libdir
/usr/local/pgsql/lib
```

Or again use pkg-config:
```bash
$ pkg-config --libs libpq
-L/usr/local/pgsql/lib -lpq
```
## Usage
### 1. For server
```bash
$ ./exec.sh server
```     
### 2. For client
```bash
$ ./exec.sh client
```     

