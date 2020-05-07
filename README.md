# knots
➰ A program for manipulating and playing with knot diagrams.

<p align="center">
  <img src="https://github.com/mwalczyk/grid-diagrams/blob/master/screenshots/screenshot.png" alt="screenshot" width="400" height="auto"/>
</p>

## Description
### Grid Diagrams
One interesting way of representing a knot is via a so-called `n`x`n` _grid diagram_. Each cell of the grid is either an `x`, an `o`, or "blank." Each row and column is restricted to have _exactly_ one `x` and one `o`. We can construct the knot corresponding to a particular grid diagram using the following procedure:

1. In each column, connect each `x` to the corresponding `o` 
2. In each row, connect each `o` to the corresponding `x`
3. Whenever a horizontal segment intersects a vertical segment, assume that the vertical segment passes _over_ the horizontal segment (i.e. a grid diagram _only_ consists of over-crossings)

### Refinement
Following the procedure outlined above results in a piecewise linear link (polyline). To obtain a "smoother" projection of the knot (one without sharp corners), we need to perform some form of topological refinement, taking care to not change the underlying structure of the knot. Following Dr. Scharein's thesis (link below), each vertex of the generated polyline is treated as a particle in a physics simulation. Adjacent vertices are attracted to one another via a mechanical spring force. Non-adjacent particles are repelled from one another via an electrostatic force, which (in practice) prevents segments from crossing over or under one another. A more robust method would perform intersection tests between all pairs of non-neighboring line segments to prevent any "illegal" crossings. This is currently a TODO item.

In order for this to work, the polyline must start in a "valid" state, where no two line segments intersect. To accomodate this, crossings are calculated based on criteria `(3)` above. A new vertex is added at each point of crossing and "lifted" along the `z`-axis by some small amount. The entire polyline is then resampled, to increase vertex density.

Before the knot is rendered, a path-guided extrusion is performed to "thicken" the knot. At each vertex along the polyline, a coordinate frame is established by calculating the tangent vector and a vector orthogonal to the tangent. Then, a circular cross-section is added at the origin of this new, local coordinate system. Adjacent cross-sections are connected with triangles to form a continuous, closed "tube." To avoid jarring rotations, [parallel transport](https://en.wikipedia.org/wiki/Parallel_transport) is employed. Essentially, each successive coordinate frame is calculated with respect to the previous frame. This ensures that the circular cross-sections smoothly rotate around the polyline during traversal.  

### Cromwell Moves
The Cromwell Moves are similar to the [Reidemeister Moves](https://en.wikipedia.org/wiki/Reidemeister_move), specifically applied to grid diagrams. They all us to obtain isotopic knots, i.e. knots that have the same underlying topology but "look" different. This gives us a way to systematically explore a given knot invariant.

There are 4 different Cromwell moves:
1. Translation: A move that cyclically translates a row or column in one of four directions: up, down, left, or right
2. Commutation: A move that exchanges to adjacent, non-interleaved rows or columns
3. Stabilization: A move that replaces an `x` with a 2x2 sub-grid
4. Destabilization: A move that replaces a 2x2 sub-grid with an `x` (the opposite of an x-stabilization): currently not supported

## Tested On
- Windows 10
- NVIDIA GeForce GTX 1660 Ti

NOTE: this project will only run on graphics cards that support OpenGL [Direct State Access](https://www.khronos.org/opengl/wiki/Direct_State_Access) (DSA). It also depends on C++17.

## To Build
1. Clone this repo and initialize submodules: 
```shell
git submodule init
git submodule update
```
2. From the root directory, run the following commands:
```shell
mkdir build
cd build
cmake ..
```
3. Open the project file for your IDE of choice (generated above)
4. Build and run the project

## To Use
All grid diagrams must be "square" `.csv` files (the same number of rows as columns). Each row and column must have _exactly_ one `x` and one `o`: all other entries should be spaces ("blank"). The grid diagram will be validated upon construction, but the program will exit if one of the conditions above is not met. An example grid diagram for the trefoil knot is shown below:

```
"x"," ","o"," "," "
" ","x"," ","o"," "
" "," ","x"," ","o"
"o"," "," ","x"," "
" ","o"," "," ","x"
```

## To Do
- [ ] Add bounding box checks (see section `7.2.2` of Scharein's thesis) to accelerate segment-segment intersection tests

### Future Directions
One interesting topic of research in knot theory is that of random knot generation. This turns out to be quite a challenging problem. How do we generate a curve that is *actually* knotted? Moreover, how do we *prove* that it is, in fact, knotted? Dr. Jason Cantarella has done significant research into this topic, which I would love to explore. Early on, I created a "naive" algorithm for randomly generating grid diagrams, but the vast majority of these diagrams resulted in the unknot. This observation prompted me to explore this problem further.

Fundamental to the problem of random knot generation is representation. More specifically, what are ways that we can represent knots (mathematically, or otherwise)? Clearly, grid diagrams are one form of representation, but there are others. One interesting algorithm is the so-called "hedgehog method," which produces random, polygonal curves. Knots can also be represented as planar graphs, which leads to the question: how can we generate random, 4-valent planar graphs? My initial research yielded [blossom trees](https://en.wikipedia.org/wiki/Blossom_tree_(graph_theory)), which are essentially trees with additional edges that are used to form closed loops in the graph. Blossom trees can be used to sample random planar graphs.

Beyond this, there are also other ways to catalog and diagram knots: Dowker codes, Conway notation, Gauss codes, braid representations, among others. Dr. Scharein (mentioned below) implemented a "tangle calculator" for building knots from small, molecular components called "tangles." He also implemented a tool for "drawing" knots by hand, which is something that I explored as well. One challenge with such a tool is: how does the user specify under-/over-crossings? In the prototype that I created, the crossings would simply alternate whenever the user crossed an existing strand. There is a lot to explore here as well!

Finally, the pseudo-physical simulation used in this program is just one way to perform topological refinement. Dr. Cantarella has also explored this topic with his "ridgerunner" software, which can "tighten" a knot via a form of constrained gradient descent. Are there other ways to accurately "relax" knots? 

## Credits
This project was largely inspired by and based on previous work done by Dr. Robert Scharein, whose PhD [thesis](https://knotplot.com/thesis/) and [software](https://knotplot.com/) were vital towards my understanding of the relaxation / meshing procedures.

### License
[Creative Commons Attribution 4.0 International License](https://creativecommons.org/licenses/by/4.0/)
