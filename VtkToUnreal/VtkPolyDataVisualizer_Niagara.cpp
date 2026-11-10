/**
 * Simplified conversion function for Niagara.
 * Extracts raw points, color map, ribbon group IDs, and link ordering from VTK.
 */
void AVtkPolyDataVisualizer::ConvertVtkPolyDataToNiagaraArrays(
    vtkPolyData* InPolyData,
    TArray<FVector>& OutPositions,
    TArray<FLinearColor>& OutColors,
    TArray<int32>& OutRibbonIDs,
    TArray<float>& OutLinkOrders)
{
    OutPositions.Empty();
    OutColors.Empty();
    OutRibbonIDs.Empty();
    OutLinkOrders.Empty();

    if (!InPolyData) return;

    vtkPoints* vtkPoints = InPolyData->GetPoints();
    vtkCellArray* vtkLines = InPolyData->GetLines();
    if (!vtkPoints || !vtkLines || vtkPoints->GetNumberOfPoints() == 0) return;

    // Determine scalar array for color mapping
    vtkDataArray* vtkScalars = InPolyData->GetPointData()->GetScalars();

    vtkIdType npts;
    const vtkIdType* pts;
    
    vtkLines->InitTraversal();
    int32 CurrentLineID = 0; // Each distinct VTK cell (line) gets a unique Ribbon ID

    // Loop through each individual line cell in the VTK file
    while (vtkLines->GetNextCell(npts, pts))
    {
        // Loop through all points belonging to this specific line
        for (int32 j = 0; j < npts; ++j)
        {
            vtkIdType PointId = pts[j];

            // 1. Get raw coordinates and scale up to Unreal Units (adjust scale as needed!)
            double coord[3];
            vtkPoints->GetPoint(PointId, coord);
            FVector UnrealPos = FVector(coord[0], coord[1], coord[2]) * 1000.0f; 

            OutPositions.Add(UnrealPos);

            // 2. Map color scalars
            if (vtkScalars)
            {
                double ScalarVal = vtkScalars->GetTuple1(PointId);
                OutColors.Add(MapScalarToColor(ScalarVal, CoolToWarmColorMapPoints, ScalarRange));
            }
            else
            {
                OutColors.Add(FLinearColor::White);
            }

            // 3. Populate Niagara Ribbon ID (All points in this line cell share the same group ID)
            OutRibbonIDs.Add(CurrentLineID);

            // 4. Populate Ribbon Link Order (The sequence order of the point: 0, 1, 2, 3...)
            OutLinkOrders.Add((float)j);                                     
        }

        CurrentLineID++; // Increment for the next distinct line cell
    }
    
    UE_LOG(LogTemp, Log, TEXT("Niagara Conversion: Processed %d ribbons, containing %d total particles."), CurrentLineID, OutPositions.Num());
}