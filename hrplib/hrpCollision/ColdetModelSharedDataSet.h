/*
 * Copyright (c) 2008, AIST, the University of Tokyo and General Robotix Inc.
 * All rights reserved. This program is made available under the terms of the
 * Eclipse Public License v1.0 which accompanies this distribution, and is
 * available at http://www.eclipse.org/legal/epl-v10.html
 * Contributors:
 * National Institute of Advanced Industrial Science and Technology (AIST)
 */
/**
   @author Shin'ichiro Nakaoka
*/


#include "ColdetModel.h"
#include "Opcode/Opcode.h"
#include <vector>

using namespace std;
using namespace hrp;

namespace hrp {

    class ColdetModelSharedDataSet
    {
    public:
        ColdetModelSharedDataSet();

        bool build();

        // need two instances ?
        Opcode::Model model;

	Opcode::MeshInterface iMesh;

	vector<IceMaths::Point> vertices;
	vector<IceMaths::IndexedTriangle> triangles;

      private:
        int refCounter;

        friend class ColdetModel;
    };
}