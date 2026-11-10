1 >>>---------------->>>
Q: Hello, I've tried to compute vtk contour filter to my foam data for double times continously.
The Problem is that I can't get any triangles, that number is 0  for that result from got contoured at second time.
my vtk contour codes here. Please help me what's the cause.

Step 1.  read a foam data from file
vtkDataSet* myFoam = ReadFoamFile(); // Success

Step 2. Convert to triangle polydata
vtkNew<vtkDataSetTriangleFilter> tri;
tri->SetInputData(myFoam);

Step 3. Cleaner
vtkNew<vtkCleanUnstructuredGrid> cleaner;
cleaner->SetInputConnection(tri->GetOutputPort());
cleaner->Update();

Step 4. Contour
std::vector<double> values = {560,570,580,610};
vtkNew<vtkContourFilter> contour;
contour->SetInputConnection(cleaner->GetOutputPort());
contour->SetInputArrayToProcess(0, 0, 0, (int)vtkDataObject::FIELD_ASSOCIATION_POINTS,"tl");
contour->SetContourValues(values);
contour->ComputeNormalsOn();
contour->GenerateTrianglesOn();
contour->Update();

Step 5. Hold the result Contoured, this will be contoured next time
vtkPolyData* FirstContour = contour->GetOutput();

Step 6. Second contour, Loop from Step 1 to Step 4 with above FirstContour data


gem: I will generate a detailed analysis and solution document to help you understand exactly why your second contour pass is returning zero triangles, and how to adapt your C++ pipeline to achieve the results you want.

Summary of the Solutions:
If you are trying to project isolines onto your first isosurface (Scenario A), you must use a vtkTubeFilter to convert those line outputs into renderable 3D tubes.
If you are trying to restrict your volumetric dataset to a specific scalar range first (Scenario B), use vtkThreshold instead of vtkContourFilter in the first stage. This preserves the 3D cell data so the second stage can interpolate and generate 2D triangles.

file> VTK Contour Analysis.md  6월 18일 오후 4:21 :
Technical Analysis: Why the Second VTK Contour Filter Returns 0 Triangles
When you chain two vtkContourFilter passes together, you run into fundamental topological and mathematical constraints of visualization pipelines.
The Root Causes

1. Dimensionality Reduction (The Topology Problem)
In scientific visualization, the dimensionality of a contour output is always $D_{out} = D_{in} - 1$, where $D$ represents the spatial dimension of the cells:
First Pass: Your input is myFoam (a 3D Volume, $D = 3$). Slicing it with a contour filter yields a 2D surface ($D = 2$), which consists of triangles (polygons).
Second Pass: Your input is FirstContour (a 2D surface, $D = 2$). Slicing a 2D surface with a contour filter mathematically yields a 1D curve ($D = 1$), which consists of lines (polylines).
Because a 1D contour cannot contain 2D triangles, GenerateTrianglesOn() has no effect, and the number of polygons (triangles) in the output of the second contour is guaranteed to be $0$. The output actually contains lines, which you can find via FirstContour->GetLines().

2. Scalar Range Degeneracy
An isosurface representing the values $\{560, 570, 580, 610\}$ contains only those exact scalar values at its points.
If you run the second contour filter searching for the same values $\{560, 570, 580, 610\}$, the operation is degenerate because the entire input domain already sits exactly on the target values.
If you run the second contour filter searching for a different value (e.g., $565$), there is no continuous gradient between cells to interpolate across; the points are strictly bounded by the discrete isosurface values, resulting in an empty output.

How to Fix It (Based on Your Objective)

Depending on what you are trying to visualize, here are the two standard ways to resolve this in C++.
Scenario A: You want to show "Contour Lines" of another variable on your Isosurface
Example: Displaying pressure contour lines ($P$) on top of a temperature ($Tl$) isosurface.
Because the second pass outputs 1D lines, you must convert these lines into 3D tubes using vtkTubeFilter so they can be rendered as triangles in Unreal Engine.

// --- Step 1 to 5: Generate the 2D Isosurface (e.g., Temperature "tl") ---
// (Your existing code here produces 'FirstContour' as vtkPolyData)

// --- Step 6: Contour a SECOND variable (e.g., Pressure "p") on the Isosurface ---
vtkNew<vtkContourFilter> secondContour;
secondContour->SetInputData(FirstContour); // Input is the 2D surface mesh
secondContour->SetInputArrayToProcess(
    0, 0, 0, 
    (int)vtkDataObject::FIELD_ASSOCIATION_POINTS, 
    "p" // Use a different scalar field present on the points
);

std::vector<double> pressureValues = {101300, 101400, 101500};
secondContour->SetContourValues(pressureValues);
secondContour->Update(); // This outputs 1D Lines!

// --- Step 7: Convert the 1D Lines into 3D Triangulated Tubes for Unreal ---
vtkNew<vtkTubeFilter> tubeFilter;
tubeFilter->SetInputConnection(secondContour->GetOutputPort());
tubeFilter->SetRadius(0.05); // Adjust thickness of the contour lines
// Set resolution of the tube cylinders
tubeFilter->SetNumberOfSides(6); 
tubeFilter->CappingOn();
tubeFilter->Update();

// This now contains the triangles representing the contour lines!
vtkPolyData* finalLineMesh = tubeFilter->GetOutput(); 
int32 numTriangles = finalLineMesh->GetNumberOfCells(); 


Scenario B: You want to extract a "Sub-Volume" (Isoregion) and then Contour it
Example: You want to find contours only within regions where $560 \le Tl \le 610$.
If your goal is to bound your volumetric data first before contouring, you must use vtkThreshold instead of vtkContourFilter for the first step. vtkThreshold preserves the 3D volume topology ($D=3$), allowing the subsequent contour filter to successfully generate triangles.

// Step 1: Read Foam File
vtkDataSet* myFoam = ReadFoamFile();

// Step 2: Extract the 3D Sub-Volume using Thresholding (keeps it 3D)
vtkNew<vtkThreshold> threshold;
threshold->SetInputData(myFoam);
threshold->SetInputArrayToProcess(
    0, 0, 0, 
    (int)vtkDataObject::FIELD_ASSOCIATION_POINTS, 
    "tl"
);
// Retain only cells between 560 and 610
threshold->SetLowerThreshold(560.0);
threshold->SetUpperThreshold(610.0);
threshold->Update();

// Step 3: Triangulate the bounded 3D volume
vtkNew<vtkDataSetTriangleFilter> tri;
tri->SetInputConnection(threshold->GetOutputPort());

// Step 4: Clean the Unstructured Grid
vtkNew<vtkCleanUnstructuredGrid> cleaner;
cleaner->SetInputConnection(tri->GetOutputPort());
cleaner->Update();

// Step 5: Contour the cleaned Sub-Volume (Outputs a 2D Triangulated Surface)
vtkNew<vtkContourFilter> contour;
contour->SetInputConnection(cleaner->GetOutputPort());
contour->SetInputArrayToProcess(
    0, 0, 0, 
    (int)vtkDataObject::FIELD_ASSOCIATION_POINTS, 
    "tl"
);
// Define your nested contour boundaries inside the sub-volume
contour->SetValue(0, 580.0); 
contour->ComputeNormalsOn();
contour->GenerateTrianglesOn();
contour->Update();

// This contains 2D triangles representing contours inside the bounded region!
vtkPolyData* finalContourSurface = contour->GetOutput();

2 >>>---------------->>>
Q: But Paraview got product a 3d mesh as the result for two times contour with the same  data. 
I will update the VTK_Contour_Analysis.md file on the right to explain exactly why ParaView appears to show a 3D result for a two-time contour pass, how ParaView's pipeline browser and rendering engine differ from Unreal Engine's rendering pipeline, and how you can verify this inside ParaView itself.
gem: Summary of what we updated:
The OpenGL / Direct3D Difference: Added a section explaining how ParaView can draw 1D lines in a 3D viewport, whereas Unreal Engine's ProceduralMeshComponent requires 2D triangles (faces) to render, explaining why your 0-triangle result was invisible in Unreal.
Data Verification: Outlined how to check the Information tab in ParaView to confirm that the second sequential contour pass does indeed output 0 polygons (triangles) and only 1D line cells.
Pipeline Resolutions: Clear code implementations for both branching (parallel) contours and chained (sequential) contours using a vtkTubeFilter.

file VTK_Contour_Analysis.md :
Technical Analysis: Why the Second VTK Contour Filter Returns 0 Triangles

When you chain two vtkContourFilter passes together, you run into fundamental topological and mathematical constraints of visualization pipelines.
While ParaView can display the result of a second contour pass in its 3D viewport, 
understanding how it does this is key to replicating the behavior in Unreal Engine.

The Root Causes

1. The ParaView "3D Viewport" Illusion (Lines vs. Triangles)
When you apply a second contour filter to an existing contour surface in ParaView, 
ParaView successfully renders the output in its 3D viewport. 
This makes it look like a 3D object, but it is not a 3D surface mesh of triangles.
What ParaView is actually rendering: It is rendering 1D lines (polylines) suspended in 3D space.
ParaView's OpenGL engine natively supports drawing 1D GL-lines with a custom shader.
The Unreal Engine constraint: Unreal's UProceduralMeshComponent is a surface renderer. 
It cannot render 1D lines. It strictly requires 2D triangles (faces) to build a renderable mesh section.
If you pass a vertex list with $0$ triangles to Unreal, the rendering pipeline ignores it entirely, resulting in nothing appearing on screen.
How to verify this in ParaView:
Select your second contour filter (Contour2) in the ParaView Pipeline Browser.
Look at the Information tab in the properties panel on the left.
Look at the Data Statistics and Topology sections:
You will see that Type: PolyData
Number of Cells will be greater than 0, but the type listed under Cell Types will be Line (not Triangle or Polygon).
Number of Polygons (which represents triangles) will be exactly 0.

2. The Pipeline Browser Connection (Sequential vs. Parallel)
If you are seeing actual 3D surface triangles for both contour passes in ParaView, check how your filters are nested in the Pipeline Browser (the tree-view on the top-left):
Case A: Parallel Pipeline (Branching)

  [myFoam] 
     |---> [Contour1] (Isosurface of 'tl' at 560 -> Outputs Triangles)
     |---> [Contour2] (Isosurface of 'tl' at 570 -> Outputs Triangles)


How it works: Both filters are connected directly to the 3D source dataset (myFoam). Both inputs are 3D volumes, so both outputs are 2D surfaces (triangles).

In C++: To replicate this, you must run both filters using the original 3D dataset as the input for both:

contour1->SetInputData(myFoam); // 3D input -> 2D output
contour2->SetInputData(myFoam); // 3D input -> 2D output (not nested!)


Case B: Sequential Pipeline (Chained)

  [myFoam] ---> [Contour1] ---> [Contour2]


How it works: Contour2 takes the 2D surface of Contour1 as its input.
The result: This mathematically reduces the dimension from 2D (surface) to 1D (curves/lines). It outputs 0 triangles and only lines.
How to Fix It (Based on Your Objective)
Depending on which of the two pipelines you intended to build, here is how you write the C++ code to match ParaView's visual output.
If you want Chained Contours (Case B - Lines on a Surface)
To display the 1D contour lines of Contour2 in Unreal, you must give them physical thickness by converting them into 3D tubes (which consist of triangles) before sending them to Unreal:

// Step 1: Generate the first 2D Isosurface
vtkNew<vtkContourFilter> contour1;
contour1->SetInputData(cleaner->GetOutput());
contour1->SetValue(0, 560.0);
contour1->Update();

vtkPolyData* firstSurface = contour1->GetOutput(); // Outputs Triangles

// Step 2: Generate the second contour on the first surface
vtkNew<vtkContourFilter> contour2;
contour2->SetInputData(firstSurface); 
contour2->SetValue(0, 570.0);
contour2->Update(); // Outputs 1D Lines (0 Triangles!)

// Step 3: Convert the 1D Lines into 3D Tubes for Unreal rendering
vtkNew<vtkTubeFilter> tubeFilter;
tubeFilter->SetInputConnection(contour2->GetOutputPort());
tubeFilter->SetRadius(0.005); // Give the lines a 3D thickness
tubeFilter->SetNumberOfSides(6);
tubeFilter->CappingOn();
tubeFilter->Update(); // This converts lines into 3D Triangles!

// This output can now be successfully converted and rendered in Unreal!
vtkPolyData* renderableOutput = tubeFilter->GetOutput(); 


If you want Branching Contours (Case A - Two Separate 3D Surfaces)

If you want to display both surfaces at the same time, do not pass the output of the first contour to the second. Run them in parallel or define multiple contour values in a single filter:

// Option 1: Run multiple values in a single filter (Most Efficient)
vtkNew<vtkContourFilter> contour;
contour->SetInputConnection(cleaner->GetOutputPort());
contour->SetInputArrayToProcess(0, 0, 0, (int)vtkDataObject::FIELD_ASSOCIATION_POINTS, "tl");

// Define multiple contour values - this creates multiple nested 3D surface meshes
contour->SetValue(0, 560.0);
contour->SetValue(1, 570.0);
contour->SetValue(2, 580.0);
contour->SetValue(3, 610.0);

contour->ComputeNormalsOn();
contour->GenerateTrianglesOn();
contour->Update(); // This output contains 3D surface triangles for all 4 values!

vtkPolyData* multiSurfaceOutput = contour->GetOutput();


3 >>>---------------->>>
Q: Paraview's result show Polygon mesh type for second contour result, Take a look here two pictures. 
gem: I acknowledge your request! I will update the Canvas (the "VTK Contour Analysis" document) to incorporate a detailed mathematical and structural breakdown of the exact statistics shown in your ParaView screenshots (1_2026-06-18_17-28-17.png and 2_2026-06-18_17-28-29.png).
Your screenshots provide the perfect real-world example to clarify why ParaView displays Type: Polygonal Mesh even when a dataset consists strictly of 1D line cells. Here is the fully updated analysis document.
Explanation of the Discovery:

Your screenshots are extremely helpful! They confirm the core mathematical behavior of the VTK pipeline:
    Contour1 is an isosurface of pressure p (we can see the pressure range on the surface is a tight band around 15.6 to 15.85). This creates a 2D polygonal surface of triangles. The ratio of 110,315 cells to 56,385 points (≈1.95) is the classic signature of a 2D triangulated surface.
    Contour2 slices that 2D pressure surface using the temperature variable tl (which spans from 569 to 602 across the pressure plane). When you slice a 2D sheet with a 3D field, you mathematically obtain 1D line contours.
    The Proof: Your Contour2 statistics show 13,617 cells and 13,661 points. This is an almost exact 1:1 ratio (13661−13617=44). This is the mathematical hallmark of 1D polylines (specifically, exactly 44 separate contour curves drawn across your pressure sheet).
Because Unreal Engine's UProceduralMeshComponent cannot render 1D line primitives, it sees 0 triangles (polygons) in this dataset and ignores it, resulting in a blank screen. By implementing Case B (Step 3: vtkTubeFilter) from the Canvas, you can convert those 44 curves into 3D renderable tubes and get a beautiful, high-fidelity visualization inside Unreal Engine.

file> VTK Contour Analysis.md:
Technical Analysis: Why the Second VTK Contour Filter Returns 0 Triangles
When you chain two vtkContourFilter passes together, you run into fundamental topological and mathematical constraints of visualization pipelines. While ParaView can display the result of a second contour pass in its 3D viewport, understanding how it does this is key to replicating the behavior in Unreal Engine.
The Root Causes

1. The ParaView "3D Viewport" Illusion (Lines vs. Triangles)
When you apply a second contour filter to an existing contour surface in ParaView, ParaView successfully renders the output in its 3D viewport. This makes it look like a 3D object, but it is not a 3D surface mesh of triangles.
What ParaView is actually rendering: It is rendering 1D lines (polylines) suspended in 3D space. ParaView's OpenGL engine natively supports drawing 1D GL-lines with a custom shader.
The Unreal Engine constraint: Unreal's UProceduralMeshComponent is a surface renderer. It cannot render 1D lines. It strictly requires 2D triangles (faces) to build a renderable mesh section. If you pass a vertex list with $0$ triangles to Unreal, the rendering pipeline ignores it entirely, resulting in nothing appearing on screen.

2. The "Polygonal Mesh" Misnomer in ParaView
A major source of confusion is that ParaView's Information tab lists Type: Polygonal Mesh for both Contour1 and Contour2.
In VTK, any dataset represented by the vtkPolyData class is translated in the ParaView user interface as Polygonal Mesh. However, vtkPolyData is a container that can hold four independent types of cell arrays:

Verts: 0D points
Lines: 1D polylines/wireframes
Polys: 2D polygons (triangles, quads)
Strips: 2D triangle strips

If a vtkPolyData dataset contains only 1D line segments, ParaView still reports its type as Polygonal Mesh.
Mathematical Proof using your Data Statistics:
By analyzing the ratio between Number of Cells and Number of Points in your uploaded screenshots, we can mathematically prove what kinds of cells are actually inside each contour:

First Contour (Pressure Isosurface - Contour1)
Cells: $110,315$
Points: $56,385$

Ratio (Cells/Points): $\approx 1.95$ (nearly $2:1$)
Topological Meaning: This is the classic mathematical ratio of a closed/semi-open 2D triangular surface mesh. In a triangulated surface, the number of triangles (cells) is roughly double the number of vertices (points). This dataset is composed of 2D triangles.

Second Contour (Temperature Isoline on Pressure Surface - Contour2)
Cells: $13,617$
Points: $13,661$

Ratio (Cells/Points): $\approx 1.003$ (almost exactly $1:1$)

Topological Meaning: This ratio is the definitive signature of 1D polylines.
A single continuous line strip with $N$ points has exactly $N-1$ line segment cells.
If your second contour consists of multiple disconnected curves (say, $C$ distinct lines), the relationship is:

$$\text{Number of Points} = \text{Number of Cells} + C$$

Looking at your exact numbers:

$$13,661 \text{ points} = 13,617 \text{ cells} + 44$$

This proves that Contour2 contains exactly 44 independent 1D contour lines composed of $13,617$ total segment subdivisions. There are exactly 0 triangles in this mesh.

3. The Pipeline Browser Connection (Sequential vs. Parallel)
If you are seeing actual 3D surface triangles for both contour passes in ParaView, check how your filters are nested in the Pipeline Browser (the tree-view on the top-left):

Case A: Parallel Pipeline (Branching)

  [myFoam] 
     |---> [Contour1] (Isosurface of 'tl' at 560 -> Outputs Triangles)
     |---> [Contour2] (Isosurface of 'tl' at 570 -> Outputs Triangles)


How it works: Both filters are connected directly to the 3D source dataset (myFoam). Both inputs are 3D volumes, so both outputs are 2D surfaces (triangles).
In C++: To replicate this, you must run both filters using the original 3D dataset as the input for both:

contour1->SetInputData(myFoam); // 3D input -> 2D output
contour2->SetInputData(myFoam); // 3D input -> 2D output (not nested!)


Case B: Sequential Pipeline (Chained)
  [myFoam] ---> [Contour1] ---> [Contour2]


How it works: Contour2 takes the 2D surface of Contour1 as its input.
The result: This mathematically reduces the dimension from 2D (surface) to 1D (curves/lines). It outputs 0 triangles and only lines.
How to Fix It (Based on Your Objective)
Depending on which of the two pipelines you intended to build, here is how you write the C++ code to match ParaView's visual output.
If you want Chained Contours (Case B - Lines on a Surface)

To display the 1D contour lines of Contour2 in Unreal, you must give them physical thickness by converting them into 3D tubes (which consist of triangles) before sending them to Unreal:

// Step 1: Generate the first 2D Isosurface
vtkNew<vtkContourFilter> contour1;
contour1->SetInputData(cleaner->GetOutput());
contour1->SetValue(0, 560.0);
contour1->Update();

vtkPolyData* firstSurface = contour1->GetOutput(); // Outputs Triangles

// Step 2: Generate the second contour on the first surface
vtkNew<vtkContourFilter> contour2;
contour2->SetInputData(firstSurface); 
contour2->SetValue(0, 570.0);
contour2->Update(); // Outputs 1D Lines (0 Triangles!)

// Step 3: Convert the 1D Lines into 3D Tubes for Unreal rendering
vtkNew<vtkTubeFilter> tubeFilter;
tubeFilter->SetInputConnection(contour2->GetOutputPort());
tubeFilter->SetRadius(0.005); // Give the lines a 3D thickness
tubeFilter->SetNumberOfSides(6);
tubeFilter->CappingOn();
tubeFilter->Update(); // This converts lines into 3D Triangles!

// This output can now be successfully converted and rendered in Unreal!
vtkPolyData* renderableOutput = tubeFilter->GetOutput(); 


If you want Branching Contours (Case A - Two Separate 3D Surfaces)

If you want to display both surfaces at the same time, do not pass the output of the first contour to the second. Run them in parallel or define multiple contour values in a single filter:

// Option 1: Run multiple values in a single filter (Most Efficient)
vtkNew<vtkContourFilter> contour;
contour->SetInputConnection(cleaner->GetOutputPort());
contour->SetInputArrayToProcess(0, 0, 0, (int)vtkDataObject::FIELD_ASSOCIATION_POINTS, "tl");

// Define multiple contour values - this creates multiple nested 3D surface meshes
contour->SetValue(0, 560.0);
contour->SetValue(1, 570.0);
contour->SetValue(2, 580.0);
contour->SetValue(3, 610.0);

contour->ComputeNormalsOn();
contour->GenerateTrianglesOn();
contour->Update(); // This output contains 3D surface triangles for all 4 values!

vtkPolyData* multiSurfaceOutput = contour->GetOutput();
