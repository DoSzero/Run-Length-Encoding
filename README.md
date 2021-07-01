# Run-Length-Encoding
Run-Length Encoding (RLE) 
# What is a RLE ?
The RLE data structure is also able to store the sign of the LS function for all undefined grid points, while the DTG has ideal linear scaling memory requirements and is adaptive in all grid directions. A data structure that combines the advantages of the RLE and DTG data structures is the hierarchical run-length encoded (H-RLE) data structure [44].

As shown in Figure 3.6 the H-RLE data structure is hierarchically organized, similar to the DTG. However, instead of storing sequences of defined grid points, which are projected to the same grid point in the lower dimensional space, run-length encoding is applied. Hence, the H-RLE data structure is able to store the sign of the LS function for all undefined grid points and, in addition, it shows the same characteristics regarding memory consumption and random access as the DTG.

The H-RLE data structure leads to an inherent segmentation of the entire grid. All grid points either belong to an undefined run or are defined grid points. Figure 3.7 shows the corresponding segmentation of the two-dimensional example given in Figure 3.3. The number of segments for the worst case is $ {{N}_\text{D}}\cdot(2{D}+1)={\mathcal{O}}({{N}_\text{D}})$ . However, if many defined grid points are neighbored, which is a common case for the LS method, the number of segments is much smaller.

# How to use ? 
```
gcc main.c
./a.out yourpicture.bmp
```
