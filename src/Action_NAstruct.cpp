// NAstruct 
//#include <cstdlib>
//#include <cstring>
#include "Action_NAstruct.h"
#include "CpptrajStdio.h"
#include "DistRoutines.h"
#include "vectormath.h"
#include <cmath>

// CONSTRUCTOR
NAstruct::NAstruct() {
  //fprintf(stderr,"NAstruct Con\n");
  Nbp=0;
  Nbases=0;
  HBcut2 = 12.25; // 3.5^2
  Ocut2 = 6.25;   // 2.5^2
  resRange=NULL;
  outFilename=NULL;
} 

// DESTRUCTOR
NAstruct::~NAstruct() { 
  if (resRange!=NULL) delete resRange;
  ClearLists();
}

/*
 * NAstruct::ClearLists()
 * Clear all parm-dependent lists
 */
void NAstruct::ClearLists() {
  while (!RefCoords.empty()) {
    delete RefCoords.back();
    RefCoords.pop_back();
  }
  while (!BaseAxes.empty()) {
    delete BaseAxes.back();
    BaseAxes.pop_back();
  }
  while (!BasePairAxes.empty()) {
    delete BasePairAxes.back();
    BasePairAxes.pop_back();
  }
  while (!ExpMasks.empty()) {
    delete ExpMasks.back();
    ExpMasks.pop_back();
  }
  while (!ExpFrames.empty()) {
    delete ExpFrames.back();
    ExpFrames.pop_back();
  }
}

// ------------------------- PRIVATE FUNCTIONS --------------------------------

/*
 * NAstruct::GCpair()
 * Look for 3 HB based on heavy atom distances:
 * 1. G:O6 -- C:N4  6 -- 6
 * 2. G:N1 -- C:N3  7 -- 4
 * 3. G:N2 -- C:O2  9 -- 3
 * Atom positions are known in standard Ref. Multiply by 3 to get into X.
 */
bool NAstruct::GCpair(AxisType *DG, AxisType *DC) {
  int Nhbonds = 0;
  double dist2; 
  dist2 = DIST2_NoImage(DG->X+18, DC->X+18);
  if ( dist2 < HBcut2 ) {
    Nhbonds++;
    mprintf("            G:O6 -- C:N4 = %lf\n",sqrt(dist2));
  }
  dist2 = DIST2_NoImage(DG->X+21, DC->X+12);
  if ( dist2 < HBcut2 ) {
    Nhbonds++;
    mprintf("            G:N1 -- C:N3 = %lf\n",sqrt(dist2));
  }
  dist2 = DIST2_NoImage(DG->X+27, DC->X+9 );
  if ( dist2 < HBcut2 ) {
    Nhbonds++;
    mprintf("            G:N2 -- C:O2 = %lf\n",sqrt(dist2));
  }
  if (Nhbonds>0) return true;
  return false;
}

/*
 * NAstruct::ATpair()
 * Look for 2 HB based on heavy atom distances
 * 1. A:N6 -- T:O4  6 -- 6
 * 2. A:N1 -- T:N3  7 -- 4
 */
bool NAstruct::ATpair(AxisType *DA, AxisType *DT) {
  int Nhbonds = 0;
  double dist2;
  dist2 = DIST2_NoImage(DA->X+18, DT->X+18);
  if ( dist2 < HBcut2 ) {
    Nhbonds++;
    mprintf("            A:N6 -- T:O4 = %lf\n",sqrt(dist2));
  }
  dist2 = DIST2_NoImage(DA->X+21, DT->X+12);
  if ( dist2 < HBcut2 ) {
    Nhbonds++;
    mprintf("            A:N1 -- T:N3 = %lf\n",sqrt(dist2));
  }
  if (Nhbonds>0) return true;
  return false;
}

/* 
 * NAstruct::basesArePaired()
 * Given two base axes for which IDs have been given and reference coords set,
 * determine whether the bases are paired via hydrogen bonding criteria.
 */
bool NAstruct::basesArePaired(AxisType *base1, AxisType *base2) {
  // G C
  if      ( base1->ID==DG && base2->ID==DC ) return GCpair(base1,base2);
  else if ( base1->ID==DC && base2->ID==DG ) return GCpair(base2,base1);
  else if ( base1->ID==DA && base2->ID==DT ) return ATpair(base1,base2);
  else if ( base1->ID==DT && base2->ID==DA ) return ATpair(base2,base1);
//  else {
//    mprintf("Warning: NAstruct: Unrecognized pair: %s - %s\n",NAbaseName[base1->ID],
//             NAbaseName[base2->ID]);
//  }
  return false;
}

/*
 * NAstruct::determineBasePairing()
 * Determine which bases are paired from the base axes.
 */
int NAstruct::determineBasePairing() {
  double distance;
  std::vector<bool> isPaired( BaseAxes.size(), false);
  int base1,base2;

  Nbp = 0;
  BasePair.clear();
  
  mprintf(" ==== Setup Base Pairing ==== \n");

  /* For each unpaired base, determine if it is paired with another base
   * determined by the distance between their axis origins.
   */
  for (base1=0; base1 < Nbases-1; base1++) {
    if (isPaired[base1]) continue;
    for (base2=base1+1; base2 < Nbases; base2++) {
      if (isPaired[base2]) continue;
      // First determine if origin axes coords are close enough to consider pairing
      // Origin is 4th coord
      distance = DIST2_NoImage(BaseAxes[base1]->X+4, BaseAxes[base2]->X+4);
      mprintf("  Axes distance for %i:%s -- %i:%s is %lf\n",
              base1,RefCoords[base1]->BaseName(),
              base2,RefCoords[base2]->BaseName(),distance);
      if (distance < Ocut2) {
        mprintf("    Checking %i:%s -- %i:%s\n",base1,RefCoords[base1]->BaseName(),
                base2,RefCoords[base2]->BaseName());
        // Figure out if z vectors point in same direction
        //distance = dot_product(BaseAxes[base1]->X+6,BaseAxes[base2]->X+6);
        //mprintf("    Dot product of Z vectors: %lf\n",distance*RADDEG);
        if (basesArePaired(RefCoords[base1], RefCoords[base2])) {
          BasePair.push_back(base1);
          BasePair.push_back(base2);
          isPaired[base1]=true;
          isPaired[base2]=true;
          Nbp++;
        }
      }
    } // END Loop over base2
  } // END Loop over base1

  Nbp2 = Nbp * 2; // Should also be actual size of BasePair

  mprintf("    NAstruct: Set up %i base pairs.\n",Nbp);
  base2=1;
  for (base1=0; base1 < Nbp2; base1+=2) {
    mprintf("        BP %i: Res %i:%s to %i:%s\n",base2++,
            BasePair[base1  ]+1, RefCoords[ BasePair[base1  ] ]->BaseName(),
            BasePair[base1+1]+1, RefCoords[ BasePair[base1+1] ]->BaseName());
  }

  return 0;
}

/*
 * NAstruct::setupBasePairAxes()
 * Given a list of base pairs and base axes, setup an 
 * Axestype structure containing reference base pair axes.
 * The axis extraction equation is based on that found in:
 *   3D game engine design: a practical approach to real-time Computer Graphics,
 *   Volume 385, By David H. Eberly, 2001, p. 16.
 */
int NAstruct::setupBasePairAxes() {
  int basepair, BP;
  double RotMatrix[9], TransVec[6], V[3], theta;
  AxisType *Base1;
  AxisType *Base2;
  // DEBUG
  int basepairaxesatom=0;
  PtrajFile basepairaxesfile;
  basepairaxesfile.SetupFile((char*)"basepairaxes.pdb",WRITE,UNKNOWN_FORMAT,UNKNOWN_TYPE,0);
  basepairaxesfile.OpenFile();
  // END DEBUG

  mprintf(" ==== Setup Base Pair Axes ==== \n");
  // Loop over all base pairs
  BP = 0;
  for (basepair = 0; basepair < Nbp2; basepair+=2) {
    // Set Base1 as a copy, this will become the base pair axes
    Base1 = new AxisType();
    Base1->SetPrincipalAxes();
    Base1->SetFromFrame( BaseAxes[ BasePair[basepair] ] );
    Base2 = BaseAxes[ BasePair[basepair+1] ];
    // Set frame coords for first base in the pair
    RefFrame.SetFromFrame( Base1 );
    // Set frame coords for second base in the pair
    ExpFrame.SetFromFrame( Base2 );
    // Flip the axes in the second pair
    // NOTE: This flip is only correct for standard WC base-pairing, not explicitly checked
    ExpFrame.FlipYZ();
    // RMS fit Axes of Base2 onto Base1
    ExpFrame.RMSD( &RefFrame, RotMatrix, TransVec, false);
    // DEBUG
    //mprintf("  BP: %i Rotation matrix/Translation vector:\n",BP+1);
    //printRotTransInfo(RotMatrix, TransVec);
    // Extract angle from resulting rotation matrix
    theta=matrix_to_angle(RotMatrix);
    mprintf("Base2 for pair %i will be rotated by %lf degrees.\n",BP+1,RADDEG*theta);
    // Calc Axis of rotation
    if (axis_of_rotation(V, RotMatrix, theta)) {
      mprintf("Error: NAstruct::setupBasePairAxes(): Could not set up axis of rotation for %i.\n",
              basepair);
      delete Base1;
      return 1;
    }
    // Calculate new half-rotation matrix
    calcRotationMatrix(RotMatrix,V,theta/2);
    /* Rotate Base2 by half rotation towards Base1.
     * Since the new rotation axis by definition is located at
     * the origin, the coordinates of the base have to be shifted, rotated,
     * then shifted back. Use V to store the shift back.
     */
    V[0] = -TransVec[0];
    V[1] = -TransVec[1];
    V[2] = -TransVec[2];
    Base2->Translate( TransVec );
    Base2->Rotate( RotMatrix );
    Base2->Translate( V );
    /* Since rotation matrix for Base2 was calculated with same origin as 
     * Base1, use reverse rotation matrix (transpose) to rotate Base1. 
     * Shift, rotate, shift back. Use V to store the shift back.
     */
    V[0] = -TransVec[3];
    V[1] = -TransVec[4];
    V[2] = -TransVec[5];
    Base1->Translate( V );
    Base1->InverseRotate( RotMatrix );
    Base1->Translate( TransVec+3 );
    // Origin of base pair axes is midpoint between Base1 and Base2 origins
    V[0] = ( (Base1->X[9 ] + Base2->X[9 ])/2 ) - Base1->X[9 ];
    V[1] = ( (Base1->X[10] + Base2->X[10])/2 ) - Base1->X[10];
    V[2] = ( (Base1->X[11] + Base2->X[11])/2 ) - Base1->X[11];
    // Shift Base1 to midpoint; Base1 becomes the base pair axes
    Base1->Translate( V );
    // DEBUG
    V[0] = Base1->X[6] - Base1->X[9];
    V[1] = Base1->X[7] - Base1->X[10];
    V[2] = Base1->X[8] - Base1->X[11];
    //normalize(V);
    // NOTE: Axes are already normalized
    mprintf("      %i) %i:%s -- %i:%s  %8.2lf %8.2lf %8.2lf %8.2lf %8.2lf %8.2lf\n",BP,
            BasePair[basepair  ], Base1->BaseName(),
            BasePair[basepair+1], Base2->BaseName(),
            Base1->X[9], Base1->X[10], Base1->X[11],
            V[0], V[1], V[2]);
    // Base1 now contains absolute coords of base pair reference axes
    Base1->WritePDB(&basepairaxesfile, BasePair[basepair], P->ResidueName(BP),&basepairaxesatom);
    BasePairAxes.push_back( Base1 );
    BP++;
  }
  // DEBUG
  basepairaxesfile.CloseFile();

  return 0;
}

/*
 * NAstruct::setupBaseAxes()
 * For each residue defined in reference coords, get the corresponding input
 * coords and fit the reference coords (and reference axes) on top of input 
 * coords. This sets up the reference axes for each base.
 */
int NAstruct::setupBaseAxes(Frame *InputFrame) {
  double rmsd, RotMatrix[9], TransVec[6];
  int base;
  AxisType Origin;
  // DEBUG
  int res = 0;
  int baseaxesatom = 0;
  int basesatom = 0;
  PtrajFile baseaxesfile;
  PtrajFile basesfile;
  baseaxesfile.SetupFile((char*)"baseaxes.pdb",WRITE,UNKNOWN_FORMAT,UNKNOWN_TYPE,0);
  baseaxesfile.OpenFile();
  basesfile.SetupFile((char*)"bases.pdb",WRITE,UNKNOWN_FORMAT,UNKNOWN_TYPE,0);
  basesfile.OpenFile();
  // END DEBUG

  // For each axis in RefCoords, use corresponding mask in ExpMasks to set 
  // up an axis for ExpCoords.
  for (base=0; base < Nbases; base++) {
    // Reset Origin coords so it is always the origin
    Origin.SetPrincipalAxes();
    // Set exp coords based on previously set-up mask
    ExpFrames[base]->SetFrameCoordsFromMask( InputFrame->X, ExpMasks[base] ); 
    /* Now that we have a set of reference coords and the corresponding input
     * coords, RMS fit the reference coords to the input coords to obtain the
     * appropriate rotation and translations that will put the reference coords 
     * on top of input (experimental) coords.
     * NOTE: The RMSD routine is destructive to coords. Need copies of frames.
     */
    RefFrame.SetFromFrame( RefCoords[base] );
    ExpFrame.SetFromFrame( ExpFrames[base] );
    rmsd = RefFrame.RMSD( &ExpFrame, RotMatrix, TransVec, false);
    mprintf("Base %i: RMS of RefCoords from ExpCoords is %lf\n",base+1,rmsd);
    // Store the Rotation matrix.
    BaseAxes[base]->StoreRotMatrix( RotMatrix );
    // DEBUG
    mprintf("         Rotation matrix/Translation vector:\n");
    printRotTransInfo(RotMatrix, TransVec);
    //AxisToPDB(&baseaxesfile, (*baseaxis), res++, &baseaxesatom);
    /* RotMatrix and TransVec now contain rotation and translation
     * that will orient refcoord to expframe. The first translation is that of
     * the reference frame to the absolute origin, the second translation is
     * that of the reference frame to the exp. coords after rotation.
     * The rotation matrix essentially contains the absolute coordinates of the
     * X, Y, and Z unit vectors of the base reference coordinates.
     */

     // Use the translation/rotation to fit principal axes in BaseAxes to experimental coords.
    BaseAxes[base]->Translate( TransVec );
    BaseAxes[base]->Rotate( RotMatrix );
    BaseAxes[base]->Translate( TransVec + 3);
    // This BaseAxis now contains the absolute coordinates of the base reference axes.
    
    // DEBUG
    mprintf("         BaseAxes origin: %8.4lf %8.4lf %8.4lf\n",
            BaseAxes[base]->X[9],BaseAxes[base]->X[10],BaseAxes[base]->X[11]);
    mprintf("         BaseAxes X vec.: %8.4lf %8.4lf %8.4lf\n",
            BaseAxes[base]->X[0 ]-BaseAxes[base]->X[9 ],
            BaseAxes[base]->X[1 ]-BaseAxes[base]->X[10],
            BaseAxes[base]->X[2 ]-BaseAxes[base]->X[11]);
    mprintf("         BaseAxes Y vec.: %8.4lf %8.4lf %8.4lf\n",
            BaseAxes[base]->X[3 ]-BaseAxes[base]->X[9 ],
            BaseAxes[base]->X[4 ]-BaseAxes[base]->X[10],
            BaseAxes[base]->X[5 ]-BaseAxes[base]->X[11]);
    mprintf("         BaseAxes Z vec.: %8.4lf %8.4lf %8.4lf\n",
            BaseAxes[base]->X[6 ]-BaseAxes[base]->X[9 ],
            BaseAxes[base]->X[7 ]-BaseAxes[base]->X[10],
            BaseAxes[base]->X[8 ]-BaseAxes[base]->X[11]);
    // DEBUG - Write base axis to file
    BaseAxes[base]->WritePDB(&baseaxesfile, res, P->ResidueName(res), &baseaxesatom);

    // Overlap ref coords onto input coords. Rotate, then translate to baseaxes origin
    RefCoords[base]->Rotate( RotMatrix );
    RefCoords[base]->Translate( BaseAxes[base]->X+9 );
    // DEBUG - Write ref coords to file
    RefCoords[base]->WritePDB(&basesfile, res, P->ResidueName(res), &basesatom);
    res++;
  }

  // DEBUG
  baseaxesfile.CloseFile();
  basesfile.CloseFile();

  return 0;
}
// ----------------------------------------------------------------------------

/*
 * NAstruct::init()
 * Expected call: nastruct [resrange <range>] [out <filename>]
 * Dataset name will be the last arg checked for. Check order is:
 *    1) Keywords
 *    2) Masks
 *    3) Dataset name
 */
int NAstruct::init() {
  char *rangeArg;
  // Get keywords
  outFilename = A->getKeyString("out",NULL);
  rangeArg = A->getKeyString("resrange",NULL); 
  resRange = A->NextArgToRange(rangeArg); 
  // Get Masks
  // Dataset
  // Add dataset to data file list

  mprintf("    NAstruct: ");
  if (resRange==NULL)
    mprintf("Scanning all NA residues");
  else
    mprintf("Scanning residues %s",rangeArg);
  if (outFilename!=NULL)
    mprintf(", output to file %s",outFilename);
  mprintf("\n");

  return 0;
}

/*
 * NAstruct::setup()
 * Set up for this parmtop. Get masks etc.
 * P is set in Action::Setup
 */
int NAstruct::setup() {
  char parmAtomName[5];
  parmAtomName[4]='\0';
  int res, refAtom, atom;
  std::list<int>::iterator residue;
  AxisType *axis; 
  AtomMask *Mask;

  // Clear all lists
  ClearLists();

  // If range arg is NULL look for all NA residues.
  if (resRange==NULL) {
    resRange = new std::list<int>();
    for (res=0; res < P->nres; res++) {
      if ( ID_base(P->ResidueName(res))!=UNKNOWN_BASE )
        resRange->push_back(res);
    }

  // Otherwise, for each residue in resRange check if it is a NA
  } else {
    residue=resRange->begin();
    while (residue!=resRange->end()) {
      // User residues numbers start from 1
      (*residue) = (*residue) - 1;
      if (ID_base(P->ResidueName(*residue))==UNKNOWN_BASE) 
        residue = resRange->erase(residue);
      else 
        residue++;
    }
  }
  // Exit if no NA residues specified
  if (resRange->empty()) {
    mprintf("Error: NAstruct::setup: No NA residues found for %s\n",P->parmName);
    return 1;
  }

  // DEBUG - print all residues
  mprintf("    NAstruct: NA res:");
  for (residue=resRange->begin(); residue!=resRange->end(); residue++)
    mprintf(" %i",(*residue)+1);
  mprintf("\n");

  // Set up frame to hold reference coords for each NA residue
  for (residue=resRange->begin(); residue!=resRange->end(); residue++) {
    axis = new AxisType();
    if ( axis->SetRefCoord( P->ResidueName(*residue) ) ) {
      mprintf("Error: NAstruct::setup: Could not get ref coords for %i:%s\n",
              (*residue)+1, P->ResidueName(*residue));
      return 1;
    }
    RefCoords.push_back( axis );

    // Set up a mask for this NA residue in this parm. The mask will contain
    // only those atoms which are defined in the reference coords.
    Mask = new AtomMask();
    for (refAtom=0; refAtom < axis->natom; refAtom++) {
      res = -1; // Target atom
      //mprintf("      Ref atom: [%s]\n",axis->Name[refAtom]);
      for (atom=P->resnums[*residue]; atom < P->resnums[(*residue)+1]; atom++) {
        // Replace any * in atom name with '
        if (P->names[atom][0]=='*') parmAtomName[0]='\''; else parmAtomName[0]=P->names[atom][0];
        if (P->names[atom][1]=='*') parmAtomName[1]='\''; else parmAtomName[1]=P->names[atom][1];
        if (P->names[atom][2]=='*') parmAtomName[2]='\''; else parmAtomName[2]=P->names[atom][2];
        if (P->names[atom][3]=='*') parmAtomName[3]='\''; else parmAtomName[3]=P->names[atom][3];
        //mprintf("        Scanning %i [%s]\n", atom, P->names[atom])i;
        if ( axis->AtomNameIs(refAtom, parmAtomName) ) {
          res = atom;  
          break;
        }
      }
      if (res==-1) {
        mprintf("Error:: NAstruct::setup: Ref atom [%s] not found in residue %i:%s\n",
                 axis->AtomName(refAtom), (*residue)+1, P->ResidueName(*residue));
        return 1;
      }
      Mask->AddAtom(res);
    } // End Loop over reference atoms
    if (Mask->None()) {
      mprintf("Error:: NAstruct::setup: No atoms found for residue %i:%s\n",
              (*residue)+1, P->ResidueName(*residue));
      delete Mask;
      return 1;
    }
    ExpMasks.push_back( Mask );
    mprintf("      NAstruct: Res %i:%s mask atoms: ",(*residue)+1,P->ResidueName(*residue));
    Mask->PrintMaskAtoms();
    mprintf("\n");

    // Set up empty frame to hold input coords for this residue
    axis = new AxisType(Mask->Nselected);
    ExpFrames.push_back( axis );

    // Set up initial axes for this NA residue.
    axis = new AxisType(); 
    axis->SetPrincipalAxes();
    BaseAxes.push_back( axis );
  } // End Loop over NA residues

  Nbases = RefCoords.size(); // Also BaseAxes, ExpFrames, and ExpMasks size.
  mprintf("    NAstruct: Set up %i bases.\n",Nbases);

  return 0;  
}

/*
 * NAstruct::action()
 */
int NAstruct::action() {

  // Set up base axes
  setupBaseAxes(F);

  // Determine Base Pairing
  determineBasePairing();

  // Get base pair axes
  setupBasePairAxes();

  return 0;
} 


