/// This is an example of solving the steady 2D Fluid-Structure Interaction problem
/// using the incompressible Navier-Stokes and nonlinear elasticity solvers
#include <gismo.h>
#include <gsElasticity/gsElasticityAssembler.h>
#include <gsElasticity/gsNsAssembler.h>
#include <gsElasticity/gsNewton.h>
#include <gsElasticity/gsWriteParaviewMultiPhysics.h>

using namespace gismo;

void refineBoundaryLayer(gsMultiBasis<> & velocity, gsMultiBasis<> & pressure)
{
    gsMatrix<> boxEast(2,2);
    boxEast << 0.8,1.,0.,0.;
    velocity.refine(0,boxEast);
    pressure.refine(0,boxEast);

    gsMatrix<> boxSouth(2,2);
    boxSouth << 0.,0.,0.,0.2;
    velocity.refine(1,boxSouth);
    velocity.refine(3,boxSouth);
    pressure.refine(1,boxSouth);
    pressure.refine(3,boxSouth);

    gsMatrix<> boxNorth(2,2);
    boxNorth << 0.,0.,0.8,1.;
    velocity.refine(2,boxNorth);
    velocity.refine(4,boxNorth);
    pressure.refine(2,boxNorth);
    pressure.refine(4,boxNorth);

    gsMatrix<> boxWest(2,2);
    boxWest << 0.,0.2,0.,0.;
    velocity.refine(5,boxWest);
    pressure.refine(5,boxWest);
}

int main(int argc, char* argv[]){
    gsInfo << "Testing the steady fluid-structure interaction solver in 2D.\n";

    //=====================================//
                // Input //
    //=====================================//

    std::string filenameFlow = ELAST_DATA_DIR"/fsi_flow_around_cylinder.xml";
    std::string filenameFlowPart = ELAST_DATA_DIR"/fsi_flow_around_cylinder_segment.xml";
    std::string filenameBeam = ELAST_DATA_DIR"/fsi_beam_around_cylinder.xml";
    index_t numUniRefFlow = 3; // number of h-refinements for the fluid
    index_t numKRefFlow = 0; // number of k-refinements for the fluid
    index_t numBLRef = 1; // number of additional boundary layer refinements for the fluid
    index_t numUniRefBeam = 3; // number of h-refinements for the beam and the ALE mapping
    index_t numKRefBeam = 0; // number of k-refinements for the beam and the ALE mapping
    index_t numPlotPoints = 10000;
    real_t youngsModulus = 1.4e6;
    real_t poissonsRatio = 0.4;
    real_t viscosity = 0.001;
    real_t maxInflow = 0.3;
    bool subgrid = false;
    bool supg = false;
    index_t iter = 3;
    real_t densityFluid = 1000.;
    real_t densitySolid = 1000.;

    // minimalistic user interface for terminal
    gsCmdLine cmd("Testing the time-dependent Stokes solver in 2D.");
    cmd.addInt("r","refine","Number of uniform refinement applications for the fluid",numUniRefFlow);
    cmd.addInt("l","blayer","Number of additional boundary layer refinements for the fluid",numBLRef);
    cmd.addInt("b","beamrefine","Number of uniform refinement applications for the beam and ALE",numUniRefBeam);
    cmd.addInt("p","plot","Number of points to plot to Paraview",numPlotPoints);
    cmd.addReal("y","young","Young's modulus of the beam materail",youngsModulus);
    cmd.addReal("v","viscosity","Viscosity of the fluid",viscosity);
    cmd.addReal("f","inflow","Maximum inflow velocity",maxInflow);
    cmd.addSwitch("e","element","True - subgrid, false - TH",subgrid);
    cmd.addSwitch("g","supg","Use SUPG stabilization",supg);
    cmd.addInt("i","iter","Number of coupling iterations",iter);
    cmd.addReal("d","density","Density of the solid",densitySolid);
    try { cmd.getValues(argc,argv); } catch (int rv) { return rv; }

    //=============================================//
          // Setting assemblers and solvers //
    //=============================================//

    // scanning geometry
    gsMultiPatch<> geoFlow;
    gsReadFile<>(filenameFlow, geoFlow);
    gsMultiPatch<> geoPart; // this is a part of the flow geometry; we deform only this part to save memory and time
    gsReadFile<>(filenameFlowPart, geoPart);
    gsMultiPatch<> geoBeam;
    gsReadFile<>(filenameBeam, geoBeam);

    // source function, rhs
    gsConstantFunction<> g(0.,0.,2);
    // inflow velocity profile
    gsFunctionExpr<> inflow(util::to_string(maxInflow) + "*4*y*(0.41-y)/0.41^2",2);

    // containers for solution as IGA functions
    gsMultiPatch<> velocity, pressure, displacement, ALE;
    // boundary conditions: flow
    gsBoundaryConditions<> bcInfoFlow;
    bcInfoFlow.addCondition(0,boundary::west,condition_type::dirichlet,&inflow,0);
    bcInfoFlow.addCondition(0,boundary::west,condition_type::dirichlet,0,1);
    for (index_t d = 0; d < 2; ++d)
    {   // no slip conditions
        bcInfoFlow.addCondition(0,boundary::east,condition_type::dirichlet,0,d);
        bcInfoFlow.addCondition(1,boundary::south,condition_type::dirichlet,0,d);
        bcInfoFlow.addCondition(1,boundary::north,condition_type::dirichlet,0,d);
        bcInfoFlow.addCondition(2,boundary::south,condition_type::dirichlet,0,d);
        bcInfoFlow.addCondition(2,boundary::north,condition_type::dirichlet,0,d);
        bcInfoFlow.addCondition(3,boundary::south,condition_type::dirichlet,0,d);
        bcInfoFlow.addCondition(3,boundary::north,condition_type::dirichlet,0,d);
        bcInfoFlow.addCondition(4,boundary::south,condition_type::dirichlet,0,d);
        bcInfoFlow.addCondition(4,boundary::north,condition_type::dirichlet,0,d);
        bcInfoFlow.addCondition(5,boundary::west,condition_type::dirichlet,0,d);
        bcInfoFlow.addCondition(6,boundary::south,condition_type::dirichlet,0,d);
        bcInfoFlow.addCondition(6,boundary::north,condition_type::dirichlet,0,d);
    }
    // boundary conditions: beam
    gsBoundaryConditions<> bcInfoBeam;
    for (index_t d = 0; d < 2; ++d)
        bcInfoBeam.addCondition(0,boundary::west,condition_type::dirichlet,0,d);
    gsFsiLoad<real_t> fSouth(geoPart,ALE,1,boundary::north,
                             velocity,pressure,4,viscosity,densityFluid);
    gsFsiLoad<real_t> fEast(geoPart,ALE,2,boundary::west,
                            velocity,pressure,5,viscosity,densityFluid);
    gsFsiLoad<real_t> fNorth(geoPart,ALE,0,boundary::south,
                             velocity,pressure,3,viscosity,densityFluid);
    bcInfoBeam.addCondition(0,boundary::south,condition_type::neumann,&fSouth);
    bcInfoBeam.addCondition(0,boundary::east,condition_type::neumann,&fEast);
    bcInfoBeam.addCondition(0,boundary::north,condition_type::neumann,&fNorth);
    // boundary conditions: flow mesh, set zero dirichlet on the entire boundary
    gsBoundaryConditions<> bcInfoALE;
    for (auto it = geoPart.bBegin(); it != geoPart.bEnd(); ++it)
        for (index_t d = 0; d < 2; ++d)
            bcInfoALE.addCondition(it->patch,it->side(),condition_type::dirichlet,0,d);

    // creating bases
    gsMultiBasis<> basisVelocity(geoFlow);
    gsMultiBasis<> basisPressure(geoFlow);
    for (index_t i = 0; i < numKRefFlow; ++i)
    {
        basisVelocity.degreeElevate();
        basisPressure.degreeElevate();
        basisVelocity.uniformRefine();
        basisPressure.uniformRefine();
    }
    for (index_t i = 0; i < numUniRefFlow; ++i)
    {
        basisVelocity.uniformRefine();
        basisPressure.uniformRefine();
    }
    // additional refinement of the boundary layer around the cylinder
    for (index_t i = 0; i < numBLRef; ++i)
        refineBoundaryLayer(basisVelocity,basisPressure);

    // additional velocity refinement for stable mixed FEM;
    // displacement is also refined to keep bases match at the fsi interface
    if (subgrid)
        basisVelocity.uniformRefine();
    else
        basisVelocity.degreeElevate();

    gsMultiBasis<> basisDisplacement(geoBeam);
    for (index_t i = 0; i < numKRefBeam; ++i)
    {
        basisDisplacement.degreeElevate();
        geoPart.degreeElevate();
        geoFlow.degreeElevate();
        basisDisplacement.uniformRefine();
        geoPart.uniformRefine();
        geoFlow.uniformRefine();
    }
    for (index_t i = 0; i < numUniRefFlow; ++i)
    {
        basisDisplacement.uniformRefine();
        geoPart.uniformRefine();
        geoFlow.uniformRefine();
    }
    gsMultiBasis<> basisALE(geoPart);

    // navier stokes assembler in the current configuration
    gsNsAssembler<real_t> nsAssembler(geoFlow,basisVelocity,basisPressure,bcInfoFlow,g);
    nsAssembler.options().setReal("Viscosity",viscosity);
    nsAssembler.options().setReal("Density",densityFluid);
    gsInfo << "Initialized Navier-Stokes system with " << nsAssembler.numDofs() << " dofs.\n";
    gsMatrix<> solutionFlow = gsMatrix<>::Zero(nsAssembler.numDofs(),1);
    // elasticity assembler: beam
    gsElasticityAssembler<real_t> elAssembler(geoBeam,basisDisplacement,bcInfoBeam,g);
    elAssembler.options().setReal("YoungsModulus",youngsModulus);
    elAssembler.options().setReal("PoissonsRatio",poissonsRatio);
    elAssembler.options().setInt("MaterialLaw",material_law::saint_venant_kirchhoff);
    gsInfo << "Initialized elasticity system with " << elAssembler.numDofs() << " dofs.\n";
    gsMatrix<> solutionBeam = gsMatrix<>::Zero(elAssembler.numDofs(),1);
    // elasticity assembler: flow mesh
    gsElasticityAssembler<real_t> aleAssembler(geoPart,basisALE,bcInfoALE,g);
    aleAssembler.options().setReal("PoissonsRatio",0.4);
    aleAssembler.options().setInt("MaterialLaw",material_law::saint_venant_kirchhoff);
    gsInfo << "Initialized elasticity system for ALE with " << aleAssembler.numDofs() << " dofs.\n";
    gsMatrix<> solutionALE = gsMatrix<>::Zero(aleAssembler.numDofs(),1);

    //=============================================//
             // Setting output and auxilary //
    //=============================================//

    // isogeometric fields (geometry + solution)
    gsField<> velocityField(nsAssembler.patches(),velocity);
    gsField<> pressureField(nsAssembler.patches(),pressure);
    gsField<> displacementField(geoBeam,displacement);
    gsField<> aleField(geoPart,ALE);
    // creating a container to plot all fields to one Paraview file
    std::map<std::string,const gsField<> *> fieldsFlow;
    fieldsFlow["Velocity"] = &velocityField;
    fieldsFlow["Pressure"] = &pressureField;
    std::map<std::string,const gsField<> *> fieldsBeam;
    fieldsBeam["Displacement"] = &displacementField;
    std::map<std::string,const gsField<> *> fieldsPart;
    fieldsPart["ALE"] = &aleField;
    // paraview collection of time steps
    gsParaviewCollection collectionFlow("fsi_steady_flow");
    gsParaviewCollection collectionBeam("fsi_steady_beam");
    gsParaviewCollection collectionFlowPart("fsi_steady_flow_part");
    // plotting initial condition
    nsAssembler.constructSolution(solutionFlow,velocity,pressure);
    elAssembler.constructSolution(solutionBeam,displacement);
    aleAssembler.constructSolution(solutionALE,ALE);
    gsWriteParaviewMultiPhysicsTimeStep(fieldsFlow,"fsi_steady_flow",collectionFlow,0,numPlotPoints);
    gsWriteParaviewMultiPhysicsTimeStep(fieldsBeam,"fsi_steady_beam",collectionBeam,0,numPlotPoints);
    gsWriteParaviewMultiPhysicsTimeStep(fieldsPart,"fsi_steady_flow_part",collectionFlowPart,0,numPlotPoints);

    //=============================================//
             // Coupled simulation //
    //=============================================//

    gsStopwatch clock;
    clock.restart();
    for (index_t i = 0; i < iter; ++i)
    {
        gsInfo << i+1 << "/" << iter << " FSI ITERATIONS\n";

        // 2. solve flow
        gsInfo << "solving flow\n";
        gsNewton<real_t> newtonFlow(nsAssembler,solutionFlow);
        newtonFlow.options().setInt("Verbosity",newton_verbosity::none);
        newtonFlow.options().setInt("Solver",linear_solver::LU);
        newtonFlow.solve();
        solutionFlow = newtonFlow.solution();
        // 3. compute drag&lift
        nsAssembler.constructSolution(solutionFlow,velocity,pressure);

        std::vector<std::pair<index_t, boxSide> > bdrySides;
        bdrySides.push_back(std::pair<index_t,index_t>(0,boxSide(boundary::east)));
        bdrySides.push_back(std::pair<index_t,index_t>(1,boxSide(boundary::south)));
        bdrySides.push_back(std::pair<index_t,index_t>(2,boxSide(boundary::north)));
        bdrySides.push_back(std::pair<index_t,index_t>(3,boxSide(boundary::south)));
        bdrySides.push_back(std::pair<index_t,index_t>(4,boxSide(boundary::north)));
        bdrySides.push_back(std::pair<index_t,index_t>(5,boxSide(boundary::west)));

        gsMatrix<> force = nsAssembler.computeForce(velocity,pressure,bdrySides);
        gsInfo << "Drag: " << force.at(0) << std::endl;
        gsInfo << "Lift: " << force.at(1) << std::endl;

        // 4. apply drag&lift (how?)
        /// done automatically
        // 5. solve beam (done)

        gsInfo << "solving beam\n";
        gsNewton<real_t> newtonBeam(elAssembler,solutionBeam);
        newtonBeam.options().setInt("Verbosity",newton_verbosity::none);
        newtonBeam.options().setInt("Solver",linear_solver::LU);
        newtonBeam.solve();
        solutionBeam = newtonBeam.solution();
        elAssembler.constructSolution(solutionBeam,displacement);

        // 5*. validation

        gsMatrix<> point(2,1);
        point << 1.,0.5;
        gsInfo << "Displacement of the beam point A:\n" << displacement.patch(0).eval(point) << std::endl;

        std::vector<gsMatrix<> > oldInterfaceDDoFs;
        for (index_t d = 0; d < geoBeam.domainDim(); ++d)
            oldInterfaceDDoFs.push_back(aleAssembler.fixedDofs(d));

        aleAssembler.setDirichletDofs(0,boundary::south,
                                       displacement.patch(0).boundary(boundary::north)->coefs());
        aleAssembler.setDirichletDofs(1,boundary::north,
                                       displacement.patch(0).boundary(boundary::south)->coefs());
        aleAssembler.setDirichletDofs(2,boundary::west,
                                       displacement.patch(0).boundary(boundary::east)->coefs());

        real_t interfaceRes = 0;
        for (index_t d = 0; d < geoBeam.domainDim(); ++d)
            interfaceRes += pow((aleAssembler.fixedDofs(d)-oldInterfaceDDoFs[d]).norm(),2);
        interfaceRes = sqrt(interfaceRes);
        gsInfo << "INTERFACE RESIDUAL " << interfaceRes << std::endl;

        // 6. compute flow mesh deformation (done)
        gsInfo << "computing ALE\n";
        gsNewton<real_t> newtonALE(aleAssembler,solutionALE);
        newtonALE.options().setInt("Verbosity",newton_verbosity::none);
        newtonALE.options().setInt("Solver",linear_solver::LU);
        newtonALE.solve();
        solutionALE = newtonALE.solution();
        nsAssembler.patches().patch(3).coefs() -= ALE.patch(0).coefs();
        nsAssembler.patches().patch(4).coefs() -= ALE.patch(1).coefs();
        nsAssembler.patches().patch(5).coefs() -= ALE.patch(2).coefs();

        aleAssembler.constructSolution(solutionALE,ALE);

        // 1. apply flow-mesh deformation
        nsAssembler.patches().patch(3).coefs() += ALE.patch(0).coefs();
        nsAssembler.patches().patch(4).coefs() += ALE.patch(1).coefs();
        nsAssembler.patches().patch(5).coefs() += ALE.patch(2).coefs();

        // 7. plot
        gsWriteParaviewMultiPhysicsTimeStep(fieldsFlow,"fsi_steady_flow",collectionFlow,i+1,numPlotPoints);
        gsWriteParaviewMultiPhysicsTimeStep(fieldsBeam,"fsi_steady_beam",collectionBeam,i+1,numPlotPoints);
        gsWriteParaviewMultiPhysicsTimeStep(fieldsPart,"fsi_steady_flow_part",collectionFlowPart,i+1,numPlotPoints);

    }
    gsInfo << "Solved in " << clock.stop() << "s.\n";
    gsInfo << "Plotting the output to the Paraview files \"fsi_steady flow.pvd\" and \"fsi_steady beam.pvd\"...\n";
    collectionFlow.save();
    collectionBeam.save();
    collectionFlowPart.save();
    return 0;
}
