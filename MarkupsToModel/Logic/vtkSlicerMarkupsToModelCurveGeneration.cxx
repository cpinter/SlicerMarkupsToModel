#include "vtkSlicerMarkupsToModelCurveGeneration.h"

// slicer includes
#include "vtkMRMLMarkupsFiducialNode.h"
#include "vtkMRMLModelNode.h"

// vtk includes
#include <vtkCleanPolyData.h>
#include <vtkDoubleArray.h>
#include <vtkNew.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtkSplineFilter.h>
#include <vtkTubeFilter.h>

//------------------------------------------------------------------------------
// constant default values defined here due to VS2013 compile issue C2864
const bool vtkSlicerMarkupsToModelCurveGeneration::TUBE_LOOP_DEFAULT = false;
const double vtkSlicerMarkupsToModelCurveGeneration::TUBE_RADIUS_DEFAULT = 1.0;
const int vtkSlicerMarkupsToModelCurveGeneration::TUBE_NUMBER_OF_SIDES_DEFAULT = 8;
const int vtkSlicerMarkupsToModelCurveGeneration::TUBE_SEGMENTS_BETWEEN_CONTROL_POINTS_DEFAULT = 5;
const int vtkSlicerMarkupsToModelCurveGeneration::POLYNOMIAL_ORDER_DEFAULT = 3;
const double vtkSlicerMarkupsToModelCurveGeneration::KOCHANEK_BIAS_DEFAULT = 0.0;
const double vtkSlicerMarkupsToModelCurveGeneration::KOCHANEK_CONTINUITY_DEFAULT = 0.0;
const double vtkSlicerMarkupsToModelCurveGeneration::KOCHANEK_TENSION_DEFAULT = 0.0;
const double vtkSlicerMarkupsToModelCurveGeneration::KOCHANEK_ENDS_COPY_NEAREST_DERIVATIVE_DEFAULT = 0.0;

//------------------------------------------------------------------------------
vtkStandardNewMacro( vtkSlicerMarkupsToModelCurveGeneration );

//------------------------------------------------------------------------------
vtkSlicerMarkupsToModelCurveGeneration::vtkSlicerMarkupsToModelCurveGeneration()
{
}

//------------------------------------------------------------------------------
vtkSlicerMarkupsToModelCurveGeneration::~vtkSlicerMarkupsToModelCurveGeneration()
{
}

//------------------------------------------------------------------------------
void vtkSlicerMarkupsToModelCurveGeneration::AllocateCurvePoints(vtkPoints* controlPoints, vtkPoints* outputPoints, int tubeSegmentsBetweenControlPoints, bool tubeLoop)
{
  // Number of points is different depending on whether the curve is a loop
  int numberControlPoints = controlPoints->GetNumberOfPoints();
  if (tubeLoop)
  {
    outputPoints->SetNumberOfPoints(numberControlPoints * tubeSegmentsBetweenControlPoints + 2);
    // two extra points are required to "close off" the loop, and ensure that the tube normals appear fully continuous
  }
  else
  {
    outputPoints->SetNumberOfPoints((numberControlPoints - 1) * tubeSegmentsBetweenControlPoints + 1);
  }
}

//------------------------------------------------------------------------------
void vtkSlicerMarkupsToModelCurveGeneration::CloseLoop(vtkPoints* outputPoints)
{
  // If looped, move the first point and add an *extra* point. This is 
  // needed in order for the curve to be continuous, otherwise the tube ends won't 
  // align properly
  double point0[3];
  outputPoints->GetPoint(0, point0);
  double point1[3];
  outputPoints->GetPoint(1, point1);
  double finalPoint[3];
  finalPoint[0] = point0[0] * 0.5 + point1[0] * 0.5;
  finalPoint[1] = point0[1] * 0.5 + point1[1] * 0.5;
  finalPoint[2] = point0[2] * 0.5 + point1[2] * 0.5;
  outputPoints->SetPoint(0, finalPoint);
  int finalIndex = outputPoints->GetNumberOfPoints() - 1;
  outputPoints->SetPoint(finalIndex, finalPoint);
}

//------------------------------------------------------------------------------
void vtkSlicerMarkupsToModelCurveGeneration::SetCardinalSplineParameters(vtkPoints* controlPoints, vtkCardinalSpline* splineX, vtkCardinalSpline* splineY, vtkCardinalSpline* splineZ, bool tubeLoop)
{
  if (tubeLoop)
  {
    splineX->ClosedOn();
    splineY->ClosedOn();
    splineZ->ClosedOn();
  }
  int numberControlPoints = controlPoints->GetNumberOfPoints();
  for (int i = 0; i < numberControlPoints; i++)
  {
    double point[3] = { 0.0, 0.0, 0.0 };
    controlPoints->GetPoint(i, point);
    splineX->AddPoint(i, point[0]);
    splineY->AddPoint(i, point[1]);
    splineZ->AddPoint(i, point[2]);
  }
}

//------------------------------------------------------------------------------
void vtkSlicerMarkupsToModelCurveGeneration::SetKochanekSplineParameters(vtkPoints* controlPoints, vtkKochanekSpline* splineX, vtkKochanekSpline* splineY, vtkKochanekSpline* splineZ, bool tubeLoop,
  double kochanekBias, double kochanekContinuity, double kochanekTension, bool kochanekEndsCopyNearestDerivatives)
{
  if (tubeLoop)
  {
    splineX->ClosedOn();
    splineY->ClosedOn();
    splineZ->ClosedOn();
  }
  splineX->SetDefaultBias(kochanekBias);
  splineY->SetDefaultBias(kochanekBias);
  splineZ->SetDefaultBias(kochanekBias);
  splineX->SetDefaultContinuity(kochanekContinuity);
  splineY->SetDefaultContinuity(kochanekContinuity);
  splineZ->SetDefaultContinuity(kochanekContinuity);
  splineX->SetDefaultTension(kochanekTension);
  splineY->SetDefaultTension(kochanekTension);
  splineZ->SetDefaultTension(kochanekTension);
  int numberControlPoints = controlPoints->GetNumberOfPoints();
  for (int i = 0; i < numberControlPoints; i++)
  {
    double point[3] = { 0.0, 0.0, 0.0 };
    controlPoints->GetPoint(i, point);
    splineX->AddPoint(i, point[0]);
    splineY->AddPoint(i, point[1]);
    splineZ->AddPoint(i, point[2]);
  }
  if (kochanekEndsCopyNearestDerivatives)
  {
    // manually set the derivative to the nearest value
    // (difference between the two nearest points). The
    // constraint mode is set to 1, this tells the spline
    // class to use our manual definition.
    // left derivative
    double point0[3];
    controlPoints->GetPoint(0, point0);
    double point1[3];
    controlPoints->GetPoint(1, point1);
    splineX->SetLeftConstraint(1);
    splineX->SetLeftValue(point1[0] - point0[0]);
    splineY->SetLeftConstraint(1);
    splineY->SetLeftValue(point1[1] - point0[1]);
    splineZ->SetLeftConstraint(1);
    splineZ->SetLeftValue(point1[2] - point0[2]);
    // right derivative
    double pointNMinus2[3];
    controlPoints->GetPoint(numberControlPoints - 2, pointNMinus2);
    double pointNMinus1[3];
    controlPoints->GetPoint(numberControlPoints - 1, pointNMinus1);
    splineX->SetRightConstraint(1);
    splineX->SetRightValue(pointNMinus1[0] - pointNMinus2[0]);
    splineY->SetRightConstraint(1);
    splineY->SetRightValue(pointNMinus1[1] - pointNMinus2[1]);
    splineZ->SetRightConstraint(1);
    splineZ->SetRightValue(pointNMinus1[2] - pointNMinus2[2]);
  }
  else
  {
    // This ("0") is the most simple mode for end derivative computation, 
    // described by documentation as using the "first/last two points".
    // Use this as the default because others would require setting the 
    // derivatives manually
    splineX->SetLeftConstraint(0);
    splineY->SetLeftConstraint(0);
    splineZ->SetLeftConstraint(0);
    splineX->SetRightConstraint(0);
    splineY->SetRightConstraint(0);
    splineZ->SetRightConstraint(0);
  }
}

//------------------------------------------------------------------------------
void vtkSlicerMarkupsToModelCurveGeneration::GetTubePolyDataFromPoints(vtkPoints* pointsToConnect, vtkPolyData* outputTube, double tubeRadius, int tubeNumberOfSides)
{
  if ( pointsToConnect == NULL )
  {
    vtkGenericWarningMacro("Points to connect is null. No model generated.");
    return;
  }

  if ( outputTube == NULL )
  {
    vtkGenericWarningMacro("Output tube poly data is null. No model generated.");
    return;
  }

  int numPoints = pointsToConnect->GetNumberOfPoints();

  vtkSmartPointer< vtkCellArray > lineCellArray = vtkSmartPointer< vtkCellArray >::New();
  lineCellArray->InsertNextCell(numPoints);
  for (int i = 0; i < numPoints; i++)
  {
    lineCellArray->InsertCellPoint(i);
  }

  vtkSmartPointer< vtkPolyData > linePolyData = vtkSmartPointer< vtkPolyData >::New();
  linePolyData->Initialize();
  linePolyData->SetPoints(pointsToConnect);
  linePolyData->SetLines(lineCellArray);

  if (tubeRadius > 0.0)
    {
    vtkSmartPointer< vtkTubeFilter > tubeSegmentFilter = vtkSmartPointer< vtkTubeFilter >::New();
    tubeSegmentFilter->SetInputData(linePolyData);
    tubeSegmentFilter->SetRadius(tubeRadius);
    tubeSegmentFilter->SetNumberOfSides(tubeNumberOfSides);
    tubeSegmentFilter->CappingOn();
    tubeSegmentFilter->Update();
    outputTube->DeepCopy(tubeSegmentFilter->GetOutput());
    }
  else
    {
    outputTube->DeepCopy(linePolyData);
    }
}

//------------------------------------------------------------------------------
// note: point *must* contain three values
void vtkSlicerMarkupsToModelCurveGeneration::GenerateSphereModel(double* point, vtkPolyData* outputSphere, double sphereRadius, int sphereNumberOfSides)
{
  if ( point == NULL )
  {
    vtkGenericWarningMacro("Input point for sphere generation is null. No model generated.");
    return;
  }

  if ( outputSphere == NULL )
  {
    vtkGenericWarningMacro("Output sphere poly data is null. No model generated.");
    return;
  }

  vtkSmartPointer< vtkSphereSource > sphereSource = vtkSmartPointer< vtkSphereSource >::New();
  sphereSource->SetRadius( sphereRadius );
  sphereSource->SetThetaResolution( sphereNumberOfSides );
  sphereSource->SetPhiResolution( sphereNumberOfSides );
  sphereSource->SetCenter( point );
  sphereSource->Update();

  outputSphere->DeepCopy( sphereSource->GetOutput() );
}

//------------------------------------------------------------------------------
void vtkSlicerMarkupsToModelCurveGeneration::GeneratePiecewiseLinearCurveModel(vtkPoints* controlPoints, vtkPolyData* outputTubePolyData,
  double tubeRadius, int tubeNumberOfSides, int tubeSegmentsBetweenControlPoints, bool tubeLoop)
{
  if (controlPoints == NULL)
  {
    vtkGenericWarningMacro("Control points are null. No model generated.");
    return;
  }

  if (outputTubePolyData == NULL)
  {
    vtkGenericWarningMacro("Output tube poly data is null. No model generated.");
    return;
  }

  int numberControlPoints = controlPoints->GetNumberOfPoints();
  
  // special cases
  if ( numberControlPoints <= 0 )
  {
    // no way to make a model from 0 points
    return;
  }

  if (numberControlPoints == 1)
  {
    vtkSlicerMarkupsToModelCurveGeneration::GenerateSphereModel( controlPoints->GetPoint( 0 ), outputTubePolyData, tubeRadius, tubeNumberOfSides );
    return;
  }

  vtkSmartPointer< vtkPoints > curvePoints = vtkSmartPointer< vtkPoints >::New();
  vtkSlicerMarkupsToModelCurveGeneration::AllocateCurvePoints(controlPoints, curvePoints, tubeSegmentsBetweenControlPoints, tubeLoop);

  // Iterate over the segments to interpolate, add all the "in-between" points
  int numberSegmentsToInterpolate;
  if (tubeLoop)
  {
    numberSegmentsToInterpolate = numberControlPoints;
  }
  else
  {
    numberSegmentsToInterpolate = numberControlPoints - 1;
  }

  int controlPointIndex = 0;
  int controlPointIndexNext = 1;
  while (controlPointIndex < numberSegmentsToInterpolate)
  {
    // find the two points to interpolate between
    double controlPointCurrent[3];
    controlPoints->GetPoint(controlPointIndex, controlPointCurrent);
    if (controlPointIndexNext >= numberControlPoints)
    {
      controlPointIndexNext = 0;
    }
    double controlPointNext[3];
    controlPoints->GetPoint(controlPointIndexNext, controlPointNext);
    // iterate to compute interpolating points
    for (int i = 0; i < tubeSegmentsBetweenControlPoints; i++)
    {
      double interpolationParam = i / (double)tubeSegmentsBetweenControlPoints;
      double curvePoint[3];
      curvePoint[0] = (1.0 - interpolationParam) * controlPointCurrent[0] + interpolationParam * controlPointNext[0];
      curvePoint[1] = (1.0 - interpolationParam) * controlPointCurrent[1] + interpolationParam * controlPointNext[1];
      curvePoint[2] = (1.0 - interpolationParam) * controlPointCurrent[2] + interpolationParam * controlPointNext[2];
      int curveIndex = controlPointIndex * tubeSegmentsBetweenControlPoints + i;
      curvePoints->SetPoint(curveIndex, curvePoint);
    }
    controlPointIndex++;
    controlPointIndexNext++;
  }
  // bring it the rest of the way to the final control point
  controlPointIndex = controlPointIndex % numberControlPoints; // if the index exceeds the max, bring back to 0
  double finalPoint[3] = { 0.0, 0.0, 0.0 };
  controlPoints->GetPoint(controlPointIndex, finalPoint);
  int finalIndex = tubeSegmentsBetweenControlPoints * numberSegmentsToInterpolate;
  curvePoints->SetPoint(finalIndex, finalPoint);

  // the last part of the curve depends on whether it is a loop or not
  if (tubeLoop)
  {
    vtkSlicerMarkupsToModelCurveGeneration::CloseLoop(curvePoints);
  }

  vtkSlicerMarkupsToModelCurveGeneration::GetTubePolyDataFromPoints(curvePoints, outputTubePolyData, tubeRadius, tubeNumberOfSides);
}

//------------------------------------------------------------------------------
void vtkSlicerMarkupsToModelCurveGeneration::GenerateCardinalSplineCurveModel(vtkPoints* controlPoints, vtkPolyData* outputTubePolyData,
  double tubeRadius, int tubeNumberOfSides, int tubeSegmentsBetweenControlPoints, bool tubeLoop)
{
  if (controlPoints == NULL)
  {
    vtkGenericWarningMacro("Control points are null. No model generated.");
    return;
  }

  if (outputTubePolyData == NULL)
  {
    vtkGenericWarningMacro("Output tube poly data is null. No model generated.");
    return;
  }

  int numberControlPoints = controlPoints->GetNumberOfPoints();
  
  // special cases
  if ( numberControlPoints <= 0 )
  {
    // no way to make a model from 0 points
    return;
  }

  if (numberControlPoints == 1)
  {
    vtkSlicerMarkupsToModelCurveGeneration::GenerateSphereModel( controlPoints->GetPoint( 0 ), outputTubePolyData, tubeRadius, tubeNumberOfSides );
    return;
  }

  if (numberControlPoints == 2)
  {
    vtkSlicerMarkupsToModelCurveGeneration::GeneratePiecewiseLinearCurveModel(controlPoints, outputTubePolyData, tubeRadius, tubeNumberOfSides, tubeSegmentsBetweenControlPoints, tubeLoop);
    return;
  }

  // Create the splines
  vtkSmartPointer< vtkCardinalSpline > splineX = vtkSmartPointer< vtkCardinalSpline >::New();
  vtkSmartPointer< vtkCardinalSpline > splineY = vtkSmartPointer< vtkCardinalSpline >::New();
  vtkSmartPointer< vtkCardinalSpline > splineZ = vtkSmartPointer< vtkCardinalSpline >::New();
  vtkSlicerMarkupsToModelCurveGeneration::SetCardinalSplineParameters(controlPoints, splineX, splineY, splineZ, tubeLoop);

  vtkSmartPointer< vtkPoints > curvePoints = vtkSmartPointer< vtkPoints >::New();
  vtkSlicerMarkupsToModelCurveGeneration::AllocateCurvePoints(controlPoints, curvePoints, tubeSegmentsBetweenControlPoints, tubeLoop);

  // Iterate over the segments to interpolate, add all the "in-between" points
  int numberSegmentsToInterpolate;
  if (tubeLoop)
  {
    numberSegmentsToInterpolate = numberControlPoints;
  }
  else
  {
    numberSegmentsToInterpolate = numberControlPoints - 1;
  }
  int controlPointIndex = 0;
  int controlPointIndexNext = 1;
  while (controlPointIndex < numberSegmentsToInterpolate)
  {
    // iterate to compute interpolating points
    for (int i = 0; i < tubeSegmentsBetweenControlPoints; i++)
    {
      double interpolationParam = controlPointIndex + i / (double)tubeSegmentsBetweenControlPoints;
      double curvePoint[3];
      curvePoint[0] = splineX->Evaluate(interpolationParam);
      curvePoint[1] = splineY->Evaluate(interpolationParam);
      curvePoint[2] = splineZ->Evaluate(interpolationParam);
      int curveIndex = controlPointIndex * tubeSegmentsBetweenControlPoints + i;
      curvePoints->SetPoint(curveIndex, curvePoint);
    }
    controlPointIndex++;
    controlPointIndexNext++;
  }
  // bring it the rest of the way to the final control point
  controlPointIndex = controlPointIndex % numberControlPoints; // if the index exceeds the max, bring back to 0
  double finalPoint[3] = { 0.0, 0.0, 0.0 };
  controlPoints->GetPoint(controlPointIndex, finalPoint);
  int finalIndex = tubeSegmentsBetweenControlPoints * numberSegmentsToInterpolate;
  curvePoints->SetPoint(finalIndex, finalPoint);

  // the last part of the curve depends on whether it is a loop or not
  if (tubeLoop)
  {
    vtkSlicerMarkupsToModelCurveGeneration::CloseLoop(curvePoints);
  }

  vtkSlicerMarkupsToModelCurveGeneration::GetTubePolyDataFromPoints(curvePoints, outputTubePolyData, tubeRadius, tubeNumberOfSides);
}

//------------------------------------------------------------------------------
void vtkSlicerMarkupsToModelCurveGeneration::GenerateKochanekSplineCurveModel(vtkPoints* controlPoints, vtkPolyData* outputTubePolyData,
  double tubeRadius, int tubeNumberOfSides, int tubeSegmentsBetweenControlPoints, bool tubeLoop,
  double kochanekBias, double kochanekContinuity, double kochanekTension, bool kochanekEndsCopyNearestDerivatives)
{
  if (controlPoints == NULL)
  {
    vtkGenericWarningMacro("Control points are null. No model generated.");
    return;
  }

  if (outputTubePolyData == NULL)
  {
    vtkGenericWarningMacro("Output tube poly data is null. No model generated.");
    return;
  }

  int numberControlPoints = controlPoints->GetNumberOfPoints();
  
  // special cases
  if ( numberControlPoints <= 0 )
  {
    // no way to make a model from 0 points
    return;
  }

  if (numberControlPoints == 1)
  {
    vtkSlicerMarkupsToModelCurveGeneration::GenerateSphereModel( controlPoints->GetPoint( 0 ), outputTubePolyData, tubeRadius, tubeNumberOfSides );
    return;
  }

  if (numberControlPoints == 2)
  {
    GeneratePiecewiseLinearCurveModel(controlPoints, outputTubePolyData, tubeRadius, tubeNumberOfSides, tubeSegmentsBetweenControlPoints, tubeLoop);
    return;
  }

  // Create the splines
  vtkSmartPointer< vtkKochanekSpline > splineX = vtkSmartPointer< vtkKochanekSpline >::New();
  vtkSmartPointer< vtkKochanekSpline > splineY = vtkSmartPointer< vtkKochanekSpline >::New();
  vtkSmartPointer< vtkKochanekSpline > splineZ = vtkSmartPointer< vtkKochanekSpline >::New();
  vtkSlicerMarkupsToModelCurveGeneration::SetKochanekSplineParameters(controlPoints, splineX, splineY, splineZ, tubeLoop,
    kochanekBias, kochanekContinuity, kochanekTension, kochanekEndsCopyNearestDerivatives);

  vtkSmartPointer< vtkPoints > curvePoints = vtkSmartPointer< vtkPoints >::New();
  vtkSlicerMarkupsToModelCurveGeneration::AllocateCurvePoints(controlPoints, curvePoints, tubeSegmentsBetweenControlPoints, tubeLoop);

  // Iterate over the segments to interpolate, add all the "in-between" points
  int numberSegmentsToInterpolate;
  if (tubeLoop)
  {
    numberSegmentsToInterpolate = numberControlPoints;
  }
  else
  {
    numberSegmentsToInterpolate = numberControlPoints - 1;
  }
  int controlPointIndex = 0;
  int controlPointIndexNext = 1;
  while (controlPointIndex < numberSegmentsToInterpolate)
  {
    // iterate to compute interpolating points
    for (int i = 0; i < tubeSegmentsBetweenControlPoints; i++)
    {
      double interpolationParam = controlPointIndex + i / (double)tubeSegmentsBetweenControlPoints;
      double curvePoint[3];
      curvePoint[0] = splineX->Evaluate(interpolationParam);
      curvePoint[1] = splineY->Evaluate(interpolationParam);
      curvePoint[2] = splineZ->Evaluate(interpolationParam);
      int curveIndex = controlPointIndex * tubeSegmentsBetweenControlPoints + i;
      curvePoints->SetPoint(curveIndex, curvePoint);
    }
    controlPointIndex++;
    controlPointIndexNext++;
  }
  // bring it the rest of the way to the final control point
  controlPointIndex = controlPointIndex % numberControlPoints; // if the index exceeds the max, bring back to 0
  double finalPoint[3] = { 0.0, 0.0, 0.0 };
  controlPoints->GetPoint(controlPointIndex, finalPoint);
  int finalIndex = tubeSegmentsBetweenControlPoints * numberSegmentsToInterpolate;
  curvePoints->SetPoint(finalIndex, finalPoint);

  // the last part of the curve depends on whether it is a loop or not
  if (tubeLoop)
  {
    vtkSlicerMarkupsToModelCurveGeneration::CloseLoop(curvePoints);
  }

  vtkSlicerMarkupsToModelCurveGeneration::GetTubePolyDataFromPoints(curvePoints, outputTubePolyData, tubeRadius, tubeNumberOfSides);
}

//------------------------------------------------------------------------------
void vtkSlicerMarkupsToModelCurveGeneration::GeneratePolynomialCurveModel(vtkPoints* points, vtkPolyData* outputTubePolyData,
  double tubeRadius, int tubeNumberOfSides, int tubeSegmentsBetweenControlPoints, bool tubeLoop,
  int polynomialOrder, vtkDoubleArray* inputPointParameters)
{
  if (points == NULL)
  {
    vtkGenericWarningMacro("Input points are null. No model generated.");
    return;
  }

  if (outputTubePolyData == NULL)
  {
    vtkGenericWarningMacro("Output tube poly data is null. No model generated.");
    return;
  }

  int numPoints = points->GetNumberOfPoints();
  
  // special cases
  if ( numPoints <= 0 )
  {
    // no way to make a model from 0 points
    return;
  }

  if (numPoints == 1)
  {
    vtkSlicerMarkupsToModelCurveGeneration::GenerateSphereModel( points->GetPoint( 0 ), outputTubePolyData, tubeRadius, tubeNumberOfSides );
    return;
  }

  if (numPoints == 2)
  {
    GeneratePiecewiseLinearCurveModel(points, outputTubePolyData, tubeRadius, tubeNumberOfSides, tubeSegmentsBetweenControlPoints, tubeLoop );
    return;
  }

  vtkSmartPointer<vtkDoubleArray> pointParameters = vtkDoubleArray::SafeDownCast(inputPointParameters);
  if (pointParameters == NULL) // if not defined, create an array based on the raw indices
  {
    pointParameters = vtkSmartPointer<vtkDoubleArray>::New();
    vtkSlicerMarkupsToModelCurveGeneration::ComputePointParametersFromIndices(points, pointParameters);
  }
  else if (pointParameters->GetNumberOfTuples() != numPoints) // check size of point parameters array for consistency
  {
    vtkGenericWarningMacro("Incorrect number of point parameters provided. Should have " << numPoints << " parameters (one for each point), but " << pointParameters->GetNumberOfTuples() << " are provided. No output created.");
    return; // this should not happen at all, so best course of action is to simply return and let the user handle this situation appropriately
  }

  // The system of equations using high-order polynomials is not well-conditioned.
  // The vtkMath implementation will usually abort with polynomial orders higher than 9.
  // Since there is also numerical instability, we decide to limit the polynomial order to 6.
  // If order higher than 6 is needed on a global fit, then another algorithm should be considered anyway.
  // If at some point we want to add support for higher order polynomials, then here are two options (from Andras):
  // 1. VNL. While the VNL code is more sophisticated, and I guess also more stable, you would probably need to
  //    limit the number of samples and normalize data that you pass to the LSQR solver to be able to compute 
  //    higher-order fits (see for example this page for related discussion:
  //    http://digital.ni.com/public.nsf/allkb/45C2016C23B3B0298525645F0073B828). 
  //    See an example how VNL is used in Plus:
  //    https://app.assembla.com/spaces/plus/subversion/source/HEAD/trunk/PlusLib/src/PlusCommon/PlusMath.cxx#ln111
  // 2. Mathematica uses different basis functions for polynomial fitting (shifted Chebyshev polynomials) instead 
  //    of basis functions that are simple powers of a variable to make the fitting more robust (the source code
  //    is available here: http://library.wolfram.com/infocenter/MathSource/6780/).
  const int maximumPolynomialOrder = 6;
  if (polynomialOrder > maximumPolynomialOrder)
  {
    vtkGenericWarningMacro("Desired polynomial order " << polynomialOrder << " is not supported. "
      << "Maximum polynomial order is " << maximumPolynomialOrder << ". "
      << "Will attempt to create polynomial order " << maximumPolynomialOrder << " instead.");
    polynomialOrder = maximumPolynomialOrder;
  }

  int numPolynomialCoefficients = polynomialOrder + 1;
  // special case, if polynomial is underdetermined, change the order of the polynomial
  std::set<double> uniquePointParameters;
  for (int i = 0; i < numPoints; i++)
    uniquePointParameters.insert(pointParameters->GetValue(i));
  int numUniquePointParameters = uniquePointParameters.size();
  if (numUniquePointParameters < numPolynomialCoefficients)
  {
    // must reduce the order of polynomial according to the amount of information is available
    numPolynomialCoefficients = numUniquePointParameters;
  }

  // independent values (parameter along the curve)
  int numIndependentValues = numPoints * numPolynomialCoefficients;
  std::vector<double> independentValues(numIndependentValues); // independent values
  independentValues.assign(numIndependentValues, 0.0);
  for (int c = 0; c < numPolynomialCoefficients; c++) // o = degree index
  {
    for (int p = 0; p < numPoints; p++) // p = point index
    {
      double value = std::pow(pointParameters->GetValue(p), c);
      independentValues[p * numPolynomialCoefficients + c] = value;
    }
  }
  std::vector<double*> independentMatrix(numPoints);
  independentMatrix.assign(numPoints, NULL);
  for (int p = 0; p < numPoints; p++)
  {
    independentMatrix[p] = &(independentValues[p * numPolynomialCoefficients]);
  }
  double** independentMatrixPtr = &(independentMatrix[0]);

  // dependent values
  const int numDimensions = 3; // this should never be changed from 3
  int numDependentValues = numPoints * numDimensions;
  std::vector<double> dependentValues(numDependentValues); // dependent values
  dependentValues.assign(numDependentValues, 0.0);
  for (int p = 0; p < numPoints; p++) // p = point index
  {
    double* currentPoint = points->GetPoint(p);
    for (int d = 0; d < numDimensions; d++) // d = dimension index
    {
      double value = currentPoint[d];
      dependentValues[p * numDimensions + d] = value;
    }
  }
  std::vector<double*> dependentMatrix(numPoints);
  dependentMatrix.assign(numPoints, NULL);
  for (int p = 0; p < numPoints; p++)
  {
    dependentMatrix[p] = &(dependentValues[p * numDimensions]);
  }
  double** dependentMatrixPtr = &(dependentMatrix[0]);

  // solution to least squares
  int totalNumberCoefficients = numDimensions*numPolynomialCoefficients;
  std::vector<double> coefficientValues(totalNumberCoefficients);
  coefficientValues.assign(totalNumberCoefficients, 0.0);
  std::vector<double*> coefficientMatrix(numPolynomialCoefficients);
  for (int c = 0; c < numPolynomialCoefficients; c++)
  {
    coefficientMatrix[c] = &(coefficientValues[c * numDimensions]);
  }
  double** coefficientMatrixPtr = &(coefficientMatrix[0]); // the solution

  // Input the forumulation into SolveLeastSquares
  vtkMath::SolveLeastSquares(numPoints, independentMatrixPtr, numPolynomialCoefficients, dependentMatrixPtr, numDimensions, coefficientMatrixPtr);

  // Use the values to generate points along the polynomial curve
  vtkSmartPointer<vtkPoints> smoothedPoints = vtkSmartPointer<vtkPoints>::New(); // points
  vtkSmartPointer< vtkCellArray > smoothedLines = vtkSmartPointer<  vtkCellArray >::New(); // lines
  int numPointsOnCurve = (numPoints - 1) * tubeSegmentsBetweenControlPoints + 1;
  smoothedLines->InsertNextCell(numPointsOnCurve); // one long continuous line
  for (int p = 0; p < numPointsOnCurve; p++) // p = point index
  {
    double pointMm[3];
    for (int d = 0; d < numDimensions; d++)
    {
      pointMm[d] = 0.0;
      for (int c = 0; c < numPolynomialCoefficients; c++)
      {
        double coefficient = coefficientValues[c * numDimensions + d];
        pointMm[d] += coefficient * std::pow((double(p) / (numPointsOnCurve - 1)), c);
      }
    }
    smoothedPoints->InsertPoint(p, pointMm);
    smoothedLines->InsertCellPoint(p);
  }

  // Convert the points to a tube model
  vtkSmartPointer< vtkPolyData >smoothedSegments = vtkSmartPointer< vtkPolyData >::New();
  smoothedSegments->Initialize();
  smoothedSegments->SetPoints(smoothedPoints);
  smoothedSegments->SetLines(smoothedLines);

  if (tubeRadius > 0.0)
    {
    vtkSmartPointer< vtkTubeFilter > tubeFilter = vtkSmartPointer< vtkTubeFilter >::New();
    tubeFilter->SetInputData(smoothedSegments);
    tubeFilter->SetRadius(tubeRadius);
    tubeFilter->SetNumberOfSides(tubeNumberOfSides);
    tubeFilter->CappingOn();
    tubeFilter->Update();
    outputTubePolyData->DeepCopy(tubeFilter->GetOutput());
    }
  else
    {
    outputTube->DeepCopy(smoothedSegments);
    }

}

//------------------------------------------------------------------------------
void vtkSlicerMarkupsToModelCurveGeneration::ComputePointParametersFromIndices(vtkPoints* points, vtkDoubleArray* pointParameters)
{
  if (points == NULL)
  {
    vtkGenericWarningMacro("Input control points are null. Returning.");
    return;
  }
  
  if (pointParameters == NULL)
  {
    vtkGenericWarningMacro("Output control point parameters are null. Returning.");
    return;
  }

  int numPoints = points->GetNumberOfPoints();
  // redundant error checking, to be safe
  if (numPoints < 2)
  {
    vtkGenericWarningMacro("Not enough points to compute polynomial parameters. Need at least 2 points but " << numPoints << " are provided.");
    return;
  }

  if (pointParameters->GetNumberOfTuples())
  {
    // this should never happen, but in case it does, output a warning
    vtkGenericWarningMacro("pointParameters already has contents. Clearing.");
    while (pointParameters->GetNumberOfTuples()) // clear contents just in case
      pointParameters->RemoveLastTuple();
  }

  for (int v = 0; v < numPoints; v++)
  {
    pointParameters->InsertNextTuple1(v / double(numPoints - 1));
    // division to clamp all values to range 0.0 - 1.0
  }
}

//------------------------------------------------------------------------------
void vtkSlicerMarkupsToModelCurveGeneration::ComputePointParametersFromMinimumSpanningTree(vtkPoints * points, vtkDoubleArray* pointParameters)
{
  if (points == NULL)
  {
    vtkGenericWarningMacro("Input points are null. Returning");
    return;
  }

  if (pointParameters == NULL)
  {
    vtkGenericWarningMacro("Output point parameters are null. Returning");
    return;
  }

  int numPoints = points->GetNumberOfPoints();
  // redundant error checking, to be safe
  if (numPoints < 2)
  {
    vtkGenericWarningMacro("Not enough points to compute polynomial parameters. Need at least 2 points but " << numPoints << " are provided.");
    return;
  }

  // vtk boost algorithms cannot be used because they are not built with 3D Slicer
  // so this is a custom implementation of:
  // 1. constructing an undirected graph as a 2D array
  // 2. Finding the two vertices that are the farthest apart
  // 3. running prim's algorithm on the graph
  // 4. extract the "trunk" path from the last vertex to the first
  // 5. based on the distance along that path, assign each vertex a polynomial parameter value

  // in the following code, two tasks are done:
  // 1. construct an undirected graph
  std::vector< double > distances(numPoints * numPoints);
  distances.assign(numPoints * numPoints, 0.0);
  // 2. find the two farthest-seperated vertices in the distances array
  int treeStartIndex = 0;
  int treeEndIndex = 0;
  double maximumDistance = 0;
  // iterate through all points
  for (int v = 0; v < numPoints; v++)
  {
    for (int u = 0; u < numPoints; u++)
    {
      double pointU[3], pointV[3];
      points->GetPoint(u, pointU);
      points->GetPoint(v, pointV);
      double distX = (pointU[0] - pointV[0]); double distXsq = distX * distX;
      double distY = (pointU[1] - pointV[1]); double distYsq = distY * distY;
      double distZ = (pointU[2] - pointV[2]); double distZsq = distZ * distZ;
      double dist3D = sqrt(distXsq + distYsq + distZsq);
      distances[v * numPoints + u] = dist3D;
      if (dist3D > maximumDistance)
      {
        maximumDistance = dist3D;
        treeStartIndex = v;
        treeEndIndex = u;
      }
    }
  }
  // use the 1D vector as a 2D vector
  std::vector< double* > graph(numPoints);
  for (int v = 0; v < numPoints; v++)
  {
    graph[v] = &(distances[v * numPoints]);
  }

  // implementation of Prim's algorithm heavily based on:
  // http://www.geeksforgeeks.org/greedy-algorithms-set-5-prims-minimum-spanning-tree-mst-2/
  std::vector< int > parent(numPoints); // Array to store constructed MST
  std::vector< double > key(numPoints);   // Key values used to pick minimum weight edge in cut
  std::vector< bool > mstSet(numPoints);  // To represent set of vertices not yet included in MST

  // Initialize all keys as INFINITE (or at least as close as we can get)
  for (int i = 0; i < numPoints; i++)
  {
    key[i] = VTK_DOUBLE_MAX;
    mstSet[i] = false;
  }

  // Always include first 1st vertex in MST.
  key[treeStartIndex] = 0.0;     // Make key 0 so that this vertex is picked as first vertex
  parent[treeStartIndex] = -1; // First node is always root of MST 

  // The MST will have numPoints vertices
  for (int count = 0; count < numPoints - 1; count++)
  {
    // Pick the minimum key vertex from the set of vertices
    // not yet included in MST
    int nextPointIndex = -1;
    double minDistance = VTK_DOUBLE_MAX;
    for (int v = 0; v < numPoints; v++)
    {
      if (mstSet[v] == false && key[v] < minDistance)
      {
        minDistance = key[v];
        nextPointIndex = v;
      }
    }

    // Add the picked vertex to the MST Set
    mstSet[nextPointIndex] = true;

    // Update key value and parent index of the adjacent vertices of
    // the picked vertex. Consider only those vertices which are not yet
    // included in MST
    for (int v = 0; v < numPoints; v++)
    {
      // graph[u][v] is non zero only for adjacent vertices of m
      // mstSet[v] is false for vertices not yet included in MST
      // Update the key only if graph[u][v] is smaller than key[v]
      if (graph[nextPointIndex][v] >= 0 &&
        mstSet[v] == false &&
        graph[nextPointIndex][v] < key[v])
      {
        parent[v] = nextPointIndex;
        key[v] = graph[nextPointIndex][v];
      }
    }
  }

  // determine the "trunk" path of the tree, from first index to last index
  std::vector< int > pathIndices;
  int currentPathIndex = treeEndIndex;
  while (currentPathIndex != -1)
  {
    pathIndices.push_back(currentPathIndex);
    currentPathIndex = parent[currentPathIndex]; // go up the tree one layer
  }

  // find the sum of distances along the trunk path of the tree
  double sumOfDistances = 0.0;
  for (unsigned int i = 0; i < pathIndices.size() - 1; i++)
  {
    sumOfDistances += graph[i][i + 1];
  }

  // check this to prevent a division by zero (in case all points are duplicates)
  if (sumOfDistances == 0)
  {
    vtkGenericWarningMacro("Minimum spanning tree path has distance zero. No parameters will be assigned. Check inputs (are there duplicate points?).");
    return;
  }

  // find the parameters along the trunk path of the tree
  std::vector< double > pathParameters;
  double currentDistance = 0.0;
  for (unsigned int i = 0; i < pathIndices.size() - 1; i++)
  {
    pathParameters.push_back(currentDistance / sumOfDistances);
    currentDistance += graph[i][i + 1];
  }
  pathParameters.push_back(currentDistance / sumOfDistances); // this should be 1.0

  // finally assign polynomial parameters to each point, and store in the output array
  if (pointParameters->GetNumberOfTuples() > 0)
  {
    // this should never happen, but in case it does, output a warning
    vtkGenericWarningMacro("pointParameters already has contents. Clearing.");
    while (pointParameters->GetNumberOfTuples()) // clear contents just in case
      pointParameters->RemoveLastTuple();
  }

  for (int i = 0; i < numPoints; i++)
  {
    int currentIndex = i;
    bool alongPath = false;
    int indexAlongPath = -1;
    for (unsigned int j = 0; j < pathIndices.size(); j++)
    {
      if (pathIndices[j] == currentIndex)
      {
        alongPath = true;
        indexAlongPath = j;
        break;
      }
    }
    while (!alongPath)
    {
      currentIndex = parent[currentIndex];
      for (unsigned int j = 0; j < pathIndices.size(); j++)
      {
        if (pathIndices[j] == currentIndex)
        {
          alongPath = true;
          indexAlongPath = j;
          break;
        }
      }
    }
    pointParameters->InsertNextTuple1(pathParameters[indexAlongPath]);
  }
}

//------------------------------------------------------------------------------
void vtkSlicerMarkupsToModelCurveGeneration::PrintSelf( ostream &os, vtkIndent indent )
{
  Superclass::PrintSelf( os, indent );
}
