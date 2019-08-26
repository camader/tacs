/*
  This file is part of TACS: The Toolkit for the Analysis of Composite
  Structures, a parallel finite-element code for structural and
  multidisciplinary design optimization.

  Copyright (C) 2010 University of Toronto
  Copyright (C) 2012 University of Michigan
  Copyright (C) 2014 Georgia Tech Research Corporation
  Additional copyright (C) 2010 Graeme J. Kennedy and Joaquim
  R.R.A. Martins All rights reserved.

  TACS is licensed under the Apache License, Version 2.0 (the
  "License"); you may not use this software except in compliance with
  the License.  You may obtain a copy of the License at
  
  http://www.apache.org/licenses/LICENSE-2.0 
*/

#ifndef TACS_PLANE_STRESS_CONSTITUTIVE_H
#define TACS_PLANE_STRESS_CONSTITUTIVE_H

#include "TACSConstitutive.h"
#include "TACSMaterialProperties.h"

/*
  This is the base class for the plane stress constitutive objects. 
  
  All objects performing plane stress analysis should utilize this class. 
*/
class TACSPlaneStressConstitutive : public TACSConstitutive {
 public:
  static const int NUM_STRESSES = 3;

  TACSPlaneStressConstitutive( TACSMaterialProperties *properties );
  ~TACSPlaneStressConstitutive();

  int getNumStresses();

  // Evaluate the stresss
  void evalStress( int elemIndex,
                   const double pt[],
                   const TacsScalar X[], 
                   const TacsScalar strain[],
                   TacsScalar stress[] );

  // Evaluate the tangent stiffness
  void evalTangentStiffness( int elemIndex,
                             const double pt[],
                             const TacsScalar X[], 
                             TacsScalar C[] );

  // Evaluate the thermal strain
  void evalThermalStrain( int elemIndex,
                          const double pt[],
                          const TacsScalar X[],
                          TacsScalar strain[] );

  // Evaluate the material density
  TacsScalar evalDensity( int elemIndex,
                          const double pt[],
                          const TacsScalar X[] );

  // Evaluate the material failure index
  TacsScalar failure( int elemIndex,
                      const double pt[], 
                      const TacsScalar X[],
                      const TacsScalar strain[] );

  // Extra info about the constitutive class
  const char *getObjectName();

 protected:
  // Materiial properties class
  TACSMaterialProperties *properties;  

 private:
  static const char *psName;
};

#endif // TACS_PLANE_STRESS_CONSTITUTIVE_H