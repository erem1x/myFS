# myFS, a simple Filesystem in c

## **Basic knowledge**

The program simulates a disk using a binary file (and a bitmap structure). A disk driver handles all the operations required and, eventually, a shell simulates a terminal, using an UNIX-like nomenclature.

## **Functioning**

Every file has an header containing info about **free blocks** (linked list), **file blocks** (linked list) and a **single global directory**

**There are two types of block**:
- **Data** block: containing information
- **Directory** block: containing a sequence of entries of the blocks inside it

The shell has a simple data editor and viewer that let the user manipulate simple files (data blocks). 

## **Usage**

`make` , then execute `./shell`. Users can see any eventual error on the shell, usually marked in red.





