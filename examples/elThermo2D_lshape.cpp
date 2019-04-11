/// This is an example of using the thermal expansion solver on a 2D multi-patch geometry
#include <gismo.h>
#include <gsElasticity/gsElThermoAssembler.h>

using namespace gismo;

int main(int argc, char* argv[]){

    gsInfo << "Testing the thermal expansion solver in 2D.\n";

    //=====================================//
                // Input //
    //=====================================//

    std::string filename = ELAST_DATA_DIR"/lshape.xml";
    index_t numUniRef = 3; // number of h-refinements
    index_t numDegElevate = 1; // number of p-refinements
    index_t numPlotPoints = 10000;

    // minimalistic user interface for terminal
    gsCmdLine cmd("Testing the thermal expansion solver in 2D.");
    cmd.addInt("r","refine","Number of uniform refinement application",numUniRef);
    cmd.addInt("d","prefine","Number of degree elevation application",numDegElevate);
    cmd.addInt("s","sample","Number of points to plot to Paraview",numPlotPoints);
    try { cmd.getValues(argc,argv); } catch (int rv) { return rv; }

    // source function, rhs
    gsConstantFunction<> gravity(0.,0.,2);

    gsConstantFunction<> temperature(200.,2);
    //gsFunctionExpr<> temperature("(y+1)*100+20",2);
    // material parameters
    real_t youngsModulus = 74e9;
    real_t poissonsRatio = 0.33;
    real_t thermalExpCoef = 11.2e-6;
    real_t initTemp = 20.;

    // boundary conditions
    gsBoundaryConditions<> bcInfo;
    bcInfo.addCondition(0,boundary::south,condition_type::dirichlet,0,0); // last number is a component (coordinate) number
    bcInfo.addCondition(0,boundary::south,condition_type::dirichlet,0,1);
    bcInfo.addCondition(1,boundary::south,condition_type::dirichlet,0,0);
    bcInfo.addCondition(1,boundary::south,condition_type::dirichlet,0,1);

    //=============================================//
                  // Assembly //
    //=============================================//

    // scanning geometry
    gsMultiPatch<> geometry;
    gsReadFile<>(filename, geometry);
    // creating basis
    gsMultiBasis<> basis(geometry);
    for (index_t i = 0; i < numDegElevate; ++i)
        basis.degreeElevate();
    for (index_t i = 0; i < numUniRef; ++i)
        basis.uniformRefine();

    // creating assembler
    gsElThermoAssembler<real_t> assembler(geometry,basis,bcInfo,gravity,temperature);
    assembler.options().setReal("YoungsModulus",youngsModulus);
    assembler.options().setReal("PoissonsRatio",poissonsRatio);
    assembler.options().setReal("InitTemp",initTemp);
    assembler.options().setReal("ThExpCoef",thermalExpCoef);
    assembler.options().setSwitch("ParamTemp",false); // tells assembler to evaluate the temperature field in the physical domain

    gsInfo<<"Assembling...\n";
    gsStopwatch clock;
    clock.restart();
    assembler.assemble();
    gsInfo << "Assembled a system (matrix and load vector) with "
           << assembler.numDofs() << " dofs in " << clock.stop() << "s.\n";

    //=============================================//
                  // Solving //
    //=============================================//

    gsInfo << "Solving...\n";
    clock.restart();
    gsSparseSolver<>::LU solver(assembler.matrix());
    gsVector<> solVector = solver.solve(assembler.rhs());
    gsInfo << "Solved the system with LU solver in " << clock.stop() <<"s.\n";

    // constructing solution as an IGA function
    gsMultiPatch<> solution;
    assembler.constructSolution(solVector,solution);

    // constructing an IGA field (geometry + solution)
    gsField<> solutionField(assembler.patches(),solution);

    gsField<> heatField(assembler.patches(),temperature);

    //=============================================//
                  // Output //
    //=============================================//

    gsInfo << "Plotting the output to the Paraview file \"lshape.pvd\"...\n";
    // creating a container to plot all fields to one Paraview file
    std::map<std::string,const gsField<> *> fields;
    fields["Deformation"] = &solutionField;
    fields["Temperature"] = &heatField;
    gsWriteParaviewMultiPhysics(fields,"lshape",numPlotPoints);
    gsInfo << "Done. Use Warp-by-Vector filter in Paraview to deform the geometry.\n";

    return 0;
}