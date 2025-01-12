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

### 2. Install GTK3

For Mac
```bash
brew install gtk+3
```

For Ubuntu
```bash
sudo apt-get install libgtk-3-dev
```

### 3. Config

```bash
pg_config --includedir
# Output: /usr/local/include
```

If you have pkg-config installed, you can run instead:
```bash
pkg-config --cflags libpq
# Output: -I/usr/local/include
```

You can find out the library directory using pg_config as well:
```bash
pg_config --libdir
# Output: /usr/local/pgsql/lib
```

Or again use pkg-config:
```bash
pkg-config --libs libpq
# Output: -L/usr/local/pgsql/lib -lpq
```

## Build

Run the Makefile to build the project:
```bash
make
```

## Usage

### 1. For server
```bash
./server
```     

### 2. For client GUI
```bash
./client_gui
```

