#include <vtkSmartPointer.h>
#include <vtkFloatArray.h>
#include <iostream>

int main() {
    // Create a vtkFloatArray
    vtkSmartPointer<vtkFloatArray> dataArray = vtkSmartPointer<vtkFloatArray>::New();
    dataArray->SetNumberOfComponents(3); // 3 components per tuple
    dataArray->SetNumberOfTuples(4); // 4 tuples

    // Fill the array with sample data
    for (int i = 0; i < 4; ++i) {
        dataArray->SetTuple3(i, i * 3.0, i * 3.0 + 1.0, i * 3.0 + 2.0);
    }

    // Enumerate and print the tuples
    int numTuples = dataArray->GetNumberOfTuples();
    int numComponents = dataArray->GetNumberOfComponents();
    double tuple[3]; // Array to hold tuple values

    for (int i = 0; i < numTuples; ++i) {
        dataArray->GetTuple(i, tuple);
        std::cout << "Tuple " << i << ": ";
        for (int j = 0; j < numComponents; ++j) {
            std::cout << tuple[j] << " ";
        }
        std::cout << std::endl;
    }

    return 0;
}