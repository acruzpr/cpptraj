#include <cstring>
#include <cstdio>
#include "Mol2File.h"
#include "CpptrajStdio.h" // To print debug info
#include "StringRoutines.h" // RemoveTrailingWhitespace

// CONSTRUCTOR
Mol2File::Mol2File() : 
  mol2debug_(0),
  mol2atoms_(0),
  mol2bonds_(0)
{}

/// Tripos Tags - must be in same order as enum type TRIPOSTAG
const char Mol2File::TRIPOSTAGTEXT[4][22]={
  "@<TRIPOS>MOLECULE",
  "@<TRIPOS>ATOM",
  "@<TRIPOS>BOND",
  "@<TRIPOS>SUBSTRUCTURE"
};

// Mol2File::IsMol2Keyword()
bool Mol2File::IsMol2Keyword(const char* key) {
  if (strncmp(key, "@<TRIPOS>",9)==0)
    return true;
  return false;
}

// Mol2File::ID_Mol2()
bool Mol2File::ID_Mol2(CpptrajFile& fileIn) {
  // NOTE: ASSUMES FILE IS ALREADY SETUP!
  if (fileIn.OpenFile()) return false;
  for (int line = 0; line < 10; line++) {
    std::string nextLine = fileIn.GetLine();
    //mprintf("DEBUG: MOL2LINE %i: [%s]\n",line,linebuffer_);
    if ( IsMol2Keyword(nextLine.c_str()) ) {
      fileIn.CloseFile();
      return true;
    }
  }
  fileIn.CloseFile();
  return false;
}

// Mol2File::ScanTo()
/** \return 0 if the tag was found, 1 if not found. */
int Mol2File::ScanTo( TRIPOSTAG tag ) {
  int tagSize = (int)strlen(TRIPOSTAGTEXT[tag]);
  while ( Gets(linebuffer_, BUF_SIZE)==0 ) {
    //mprintf("DEBUG: Line [%s]\n",buffer);
    //mprintf("DEBUG: Targ [%s]\n",TRIPOSTAGTEXT[tag]); 
    if (strncmp(linebuffer_, TRIPOSTAGTEXT[tag], tagSize)==0) return 0;
  }
  // Suppress this warning so routine can be used to scan # frames
  //mprintf("Warning: Mol2File::ScanTo(): Could not find tag %s\n",TRIPOSTAGTEXT[tag]);
  return 1;
}

void Mol2File::WriteHeader( Mol2File::TRIPOSTAG tag ) {
  Printf("%s\n", TRIPOSTAGTEXT[tag] );
}

// Mol2File::ReadMolecule()
/** Set title, number of atoms, and number of bonds. */
bool Mol2File::ReadMolecule( ) {
  // Scan to the section
  if ( ScanTo( MOLECULE ) == 1 ) return true;
  //   Scan title
  if ( Gets(linebuffer_, BUF_SIZE) ) return true;
  mol2title_.assign( linebuffer_ );
  RemoveTrailingWhitespace( mol2title_ );
  if (mol2debug_>0) mprintf("      Mol2 Title: [%s]\n",mol2title_.c_str());
  //   Scan # atoms and bonds
  // num_atoms [num_bonds [num_subst [num_feat [num_sets]]]]
  if ( Gets(linebuffer_, BUF_SIZE) ) return true;
  mol2atoms_ = 0;
  mol2bonds_ = 0;
  if (sscanf(linebuffer_,"%i %i",&mol2atoms_, &mol2bonds_) != 2) {
    mprinterr("Error: Mol2File: Could not read # atoms/ # bonds.\n");
    return false;
  }
  if (mol2debug_>0) {
    mprintf("\tMol2 #atoms: %i\n",mol2atoms_);
    mprintf("\tMol2 #bonds: %i\n",mol2bonds_);
  }
  return false;
}

bool Mol2File::WriteMolecule(bool hasCharges) {
  Printf("%s\n", TRIPOSTAGTEXT[MOLECULE]);
  // mol_name
  // num_atoms [num_bonds [num_subst [num_feat [num_sets]]]]
  // mol_type
  // charge_type
  // [status_bits
  // [mol_comment]]
  Printf("%s\n", mol2title_.c_str());
  Printf("%5i %5i %5i %5i %5i\n", mol2atoms_, mol2bonds_, 1, 0, 0);
  Printf("SMALL\n"); // May change this later
  if ( hasCharges )
    Printf("USER_CHARGES\n"); // May change this later
  else
    Printf("NO_CHARGES\n");
  Printf("\n\n");
  return false;
}

/** \return # atoms in next MOLECULE, -1 on error or end of file. */
int Mol2File::NextMolecule( ) {
  int natom = 0;
  // Scan to the section
  if ( ScanTo( MOLECULE ) == 1 ) return -1;
  // Scan past the title
  if ( Gets(linebuffer_, BUF_SIZE) ) return -1;
  // Scan # atoms
  if ( Gets(linebuffer_, BUF_SIZE) ) return -1;
  sscanf(linebuffer_, "%i", &natom);
  return natom;
}

// Mol2File::Mol2Bond()
int Mol2File::Mol2Bond(int& at1, int& at2) {
  if ( Gets(linebuffer_, BUF_SIZE) != 0 ) return 1;
  // bond_id origin_atom_id target_atom_id bond_type [status_bits]
  sscanf(linebuffer_,"%*i %i %i\n", &at1, &at2);
  return 0;
}

// Mol2File::Mol2XYZ()
int Mol2File::Mol2XYZ(double *X) {
  if ( Gets(linebuffer_, BUF_SIZE) != 0 ) return 1;
  sscanf(linebuffer_,"%*i %*s %lf %lf %lf",X, X+1, X+2);
  return 0;
}

// Mol2File::Mol2Atom()
Atom Mol2File::Mol2Atom() {
  char mol2name[10], mol2type[10];
  double mol2q;
  // atom_id atom_name x y z atom_type [subst_id [subst_name [charge [status_bit]]]]
  sscanf(linebuffer_, "%*i %s %*f %*f %*f %s %*i %*s %lf", mol2name, mol2type, &mol2q);
  NameType m2name( mol2name );
  // Replace all asterisks with single quote.
  m2name.ReplaceAsterisk();
  return Atom( m2name, mol2type, mol2q );
}

// Mol2File::Mol2Residue()
Residue Mol2File::Mol2Residue() {
  char resname[10];
  int resnum;
  sscanf(linebuffer_,"%*i %*s %*f %*f %*f %*s %i %s",&resnum,resname);
  NameType rname( resname );
  // Replace all asterisks with single quote.
  rname.ReplaceAsterisk();
  return Residue(resnum, rname);
}
