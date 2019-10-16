#  This file is part of TACS: The Toolkit for the Analysis of Composite
#  Structures, a parallel finite-element code for structural and
#  multidisciplinary design optimization.
#
#  Copyright (C) 2014 Georgia Tech Research Corporation
#
#  TACS is licensed under the Apache License, Version 2.0 (the
#  "License"); you may not use this software except in compliance with
#  the License.  You may obtain a copy of the License at
#
#  http://www.apache.org/licenses/LICENSE-2.0

# distutils: language=c++

# For the use of MPI
from mpi4py.libmpi cimport *
cimport mpi4py.MPI as MPI

# Import numpy
import numpy as np
cimport numpy as np

# Ensure that numpy is initialized
np.import_array()

# Import the definition required for const strings from libc.string cimport const_char

# Import C methods for python
from cpython cimport PyObject, Py_INCREF

# Import the definitions
from TACS cimport *
from constitutive cimport *
from elements cimport *

# Include the definitions
include "TacsDefs.pxi"

# Include the mpi4py header
cdef extern from "mpi-compat.h":
    pass

# A generic wrapper class for the TACSElement object
cdef class Element:
    '''Base element class'''
    def __cinit__(self, *args, **kwargs):
        self.ptr = NULL
        return

    def __dealloc__(self):
        if self.ptr != NULL:
            self.ptr.decref()

    def setComponentNum(self, int comp_num):
        if self.ptr != NULL:
            self.ptr.setComponentNum(comp_num)
        return

    def getNumNodes(self):
        if self.ptr != NULL:
            return self.ptr.getNumNodes()
        return 0;

cdef class ElementBasis:
    def __cinit__(self):
        self.ptr = NULL
        return

    def __dealloc__(self):
        if self.ptr != NULL:
            self.ptr.decref()

    def getNumNodes(self):
        if self.ptr != NULL:
            return self.ptr.getNumNodes()
        return 0

cdef class ElementModel:
    def __cinit__(self):
        self.ptr = NULL
        return

    def __dealloc__(self):
        if self.ptr != NULL:
            self.ptr.decref()

    def getSpatialDim(self):
        if self.ptr != NULL:
            return self.ptr.getSpatialDim()
        return 0

    def getVarsPerNode(self):
        if self.ptr != NULL:
            return self.ptr.getVarsPerNode()
        return 0


cdef class LinearTetrahedralBasis(ElementBasis):
    def __cinit__(self):
        self.ptr = new TACSLinearTetrahedralBasis()
        self.ptr.incref()

cdef class QuadraticTetrahedralBasis(ElementBasis):
    def __cinit__(self):
        self.ptr = new TACSQuadraticTetrahedralBasis()
        self.ptr.incref()

cdef class CubicTetrahedralBasis(ElementBasis):
    def __cinit__(self):
        self.ptr = new TACSCubicTetrahedralBasis()
        self.ptr.incref()


cdef class LinearHexaBasis(ElementBasis):
    def __cinit__(self):
        self.ptr = new TACSLinearHexaBasis()
        self.ptr.incref()

cdef class QuadraticHexaBasis(ElementBasis):
    def __cinit__(self):
        self.ptr = new TACSQuadraticHexaBasis()
        self.ptr.incref()

cdef class CubicHexaBasis(ElementBasis):
    def __cinit__(self):
        self.ptr = new TACSCubicHexaBasis()
        self.ptr.incref()


cdef class LinearQuadBasis(ElementBasis):
    def __cinit__(self):
        self.ptr = new TACSLinearQuadBasis()
        self.ptr.incref()

cdef class QuadraticQuadBasis(ElementBasis):
    def __cinit__(self):
        self.ptr = new TACSQuadraticQuadBasis()
        self.ptr.incref()

cdef class CubicQuadBasis(ElementBasis):
    def __cinit__(self):
        self.ptr = new TACSCubicQuadBasis()
        self.ptr.incref()


cdef class LinearTriangleBasis(ElementBasis):
    def __cinit__(self):
        self.ptr = new TACSLinearTriangleBasis()
        self.ptr.incref()

cdef class QuadraticTriangleBasis(ElementBasis):
    def __cinit__(self):
        self.ptr = new TACSQuadraticTriangleBasis()
        self.ptr.incref()

cdef class CubicTriangleBasis(ElementBasis):
    def __cinit__(self):
        self.ptr = new TACSCubicTriangleBasis()
        self.ptr.incref()


cdef class HeatConduction2D(ElementModel):
    def __cinit__(self, PlaneStressConstitutive con):
        self.ptr = new TACSHeatConduction2D(con.cptr)
        self.ptr.incref()

cdef class LinearElasticity2D(ElementModel):
    def __cinit__(self, PlaneStressConstitutive con):
        self.ptr = new TACSLinearElasticity2D(con.cptr, TACS_LINEAR_STRAIN)
        self.ptr.incref()

cdef class LinearThermoelasticity2D(ElementModel):
    def __cinit__(self, PlaneStressConstitutive con):
        self.ptr = new TACSLinearThermoelasticity2D(con.cptr, TACS_LINEAR_STRAIN)
        self.ptr.incref()


cdef class HeatConduction3D(ElementModel):
    def __cinit__(self, SolidConstitutive con):
        self.ptr = new TACSHeatConduction3D(con.cptr)
        self.ptr.incref()

cdef class LinearElasticity3D(ElementModel):
    def __cinit__(self, SolidConstitutive con):
        self.ptr = new TACSLinearElasticity3D(con.cptr, TACS_LINEAR_STRAIN)
        self.ptr.incref()

cdef class LinearThermoelasticity3D(ElementModel):
    def __cinit__(self, SolidConstitutive con):
        self.ptr = new TACSLinearThermoelasticity3D(con.cptr, TACS_LINEAR_STRAIN)
        self.ptr.incref()


cdef class Element2D(Element):
    def __cinit__(self, ElementModel model, ElementBasis basis):
        self.ptr = new TACSElement2D(model.ptr, basis.ptr)
        self.ptr.incref()

cdef class Element3D(Element):
    def __cinit__(self, ElementModel model, ElementBasis basis):
        self.ptr = new TACSElement3D(model.ptr, basis.ptr)
        self.ptr.incref()



# cdef class GibbsVector:
#     cdef TACSGibbsVector *ptr
#     def __cinit__(self, x, y, z):
#         self.ptr = new TACSGibbsVector(x, y, z)
#         self.ptr.incref()

#     def __dealloc__(self):
#         self.ptr.decref()
#         return

# cdef class RefFrame:
#     cdef TACSRefFrame *ptr
#     def __cinit__(self, GibbsVector r0, GibbsVector r1, GibbsVector r2):
#         self.ptr = new TACSRefFrame(r0.ptr, r1.ptr, r2.ptr)
#         self.ptr.incref()
#         return

#     def __dealloc__(self):
#         self.ptr.decref()
#         return

# cdef class RigidBody(Element):
#     cdef TACSRigidBody *rbptr
#     def __cinit__(self, RefFrame frame, TacsScalar mass,
#                   np.ndarray[TacsScalar, ndim=1, mode='c'] cRef,
#                   np.ndarray[TacsScalar, ndim=1, mode='c'] JRef,
#                   GibbsVector r0,
#                   GibbsVector v0, GibbsVector omega0, GibbsVector g,
#                   int mdv=-1,
#                   np.ndarray[int, ndim=1, mode='c'] cdvs=None,
#                   np.ndarray[int, ndim=1, mode='c'] Jdvs=None):
#         cdef int *_cdvs = NULL
#         cdef int *_Jdvs = NULL

#         # Assign the the variable numbers if they are supplied by the
#         # user
#         if cdvs is not None:
#             _cdvs = <int*>cdvs.data
#         if Jdvs is not None:
#             _Jdvs = <int*>Jdvs.data

#         # Allocate the rigid body object and set the design variables
#         self.rbptr = new TACSRigidBody(frame.ptr, mass,
#                                        <TacsScalar*>cRef.data,
#                                        <TacsScalar*>JRef.data, r0.ptr,
#                                        v0.ptr, omega0.ptr, g.ptr)
#         self.rbptr.setDesignVarNums(mdv, _cdvs, _Jdvs)

#         # Increase the reference count to the underlying object
#         self.ptr = self.rbptr
#         self.ptr.incref()
#         return

#     def setVisualization(self, RigidBodyViz viz):
#         self.rbptr.setVisualization(viz.ptr)

#     def __dealloc__(self):
#         self.ptr.decref()
#         return

#     def getNumNodes(self):
#         return self.ptr.numNodes()

#     def setComponentNum(self, int comp_num):
#         self.ptr.setComponentNum(comp_num)
#         return

# cdef class FixedConstraint(Element):
#     def __cinit__(self,
#                   GibbsVector point,
#                   RigidBody bodyA):
#         self.ptr = new TACSFixedConstraint(bodyA.rbptr,
#                                            point.ptr)
#         self.ptr.incref()
#         return

#     def __dealloc__(self):
#         self.ptr.decref()
#         return

#     def getNumNodes(self):
#         return self.ptr.numNodes()

#     def setComponentNum(self, int comp_num):
#         self.ptr.setComponentNum(comp_num)
#         return

# cdef class SphericalConstraint(Element):
#     def __cinit__(self,
#                   GibbsVector point,
#                   RigidBody bodyA, RigidBody bodyB=None):
#         if bodyB is None:
#             self.ptr = new TACSSphericalConstraint(bodyA.rbptr,
#                                                    point.ptr)
#         else:
#             self.ptr = new TACSSphericalConstraint(bodyA.rbptr, bodyB.rbptr,
#                                                    point.ptr)
#         self.ptr.incref()
#         return
#     def __dealloc__(self):
#         self.ptr.decref()
#         return
#     def numNodes(self):
#         return self.ptr.numNodes()
#     def setComponentNum(self, int comp_num):
#         self.ptr.setComponentNum(comp_num)
#         return

# cdef class RevoluteConstraint(Element):
#     def __cinit__(self, GibbsVector point, GibbsVector eA,
#                   RigidBody bodyA, RigidBody bodyB=None):
#         if bodyB is None:
#             self.ptr = new TACSRevoluteConstraint(bodyA.rbptr,
#                                                   point.ptr, eA.ptr)
#         else:
#             self.ptr = new TACSRevoluteConstraint(bodyA.rbptr, bodyB.rbptr,
#                                                   point.ptr, eA.ptr)
#         self.ptr.incref()
#         return
#     def __dealloc__(self):
#         self.ptr.decref()
#         return
#     def numNodes(self):
#         return self.ptr.numNodes()
#     def setComponentNum(self, int comp_num):
#         self.ptr.setComponentNum(comp_num)
#         return

# cdef class CylindricalConstraint(Element):
#     def __cinit__(self, GibbsVector point, GibbsVector eA,
#                   RigidBody bodyA, RigidBody bodyB=None):
#         if bodyB is None:
#             self.ptr = new TACSCylindricalConstraint(bodyA.rbptr,
#                                                      point.ptr, eA.ptr)
#         else:
#             self.ptr = new TACSCylindricalConstraint(bodyA.rbptr, bodyB.rbptr,
#                                                      point.ptr, eA.ptr)
#         self.ptr.incref()
#         return
#     def __dealloc__(self):
#         self.ptr.decref()
#         return
#     def numNodes(self):
#         return self.ptr.numNodes()
#     def setComponentNum(self, int comp_num):
#         self.ptr.setComponentNum(comp_num)
#         return

# cdef class RigidLink(Element):
#     def __cinit__(self, RigidBody bodyA):
#         self.ptr = new TACSRigidLink(bodyA.rbptr)
#         self.ptr.incref()
#         return
#     def __dealloc__(self):
#         self.ptr.decref()
#         return
#     def numNodes(self):
#         return self.ptr.numNodes()
#     def setComponentNum(self, int comp_num):
#         self.ptr.setComponentNum(comp_num)
#         return

# cdef class RevoluteDriver(Element):
#     def __cinit__(self, GibbsVector rev, TacsScalar omega):
#         self.ptr = new TACSRevoluteDriver(rev.ptr, omega)
#         self.ptr.incref()
#         return
#     def __dealloc__(self):
#         self.ptr.decref()
#         return
#     def numNodes(self):
#         return self.ptr.numNodes()
#     def setComponentNum(self, int comp_num):
#         self.ptr.setComponentNum(comp_num)
#         return

# cdef class MotionDriver(Element):
#     def __cinit__(self, GibbsVector dir, TacsScalar omega,
#                   arrest_rot=False):
#         if arrest_rot is False:
#             self.ptr = new TACSMotionDriver(dir.ptr, omega, 0)
#         else:
#             self.ptr = new TACSMotionDriver(dir.ptr, omega, 1)
#         self.ptr.incref()
#         return
#     def __dealloc__(self):
#         self.ptr.decref()
#         return
#     def numNodes(self):
#         return self.ptr.numNodes()
#     def setComponentNum(self, int comp_num):
#         self.ptr.setComponentNum(comp_num)
#         return

# cdef class AverageConstraint(Element):
#     def __cinit__(self, RigidBody body, GibbsVector point,
#                   RefFrame frame, int moment_flag=0):
#         self.ptr = new TACSAverageConstraint(body.rbptr, point.ptr,
#                                              frame.ptr, moment_flag)
#         self.ptr.incref()
#         return
#     def __dealloc__(self):
#         self.ptr.decref()
#         return
