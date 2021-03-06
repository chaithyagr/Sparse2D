/******************************************************************************
**                   Copyright (C) 2000 by CEA
*******************************************************************************
**
**    UNIT
**
**    Version: 1.0
**
**    Author: Jean-Luc Starck
**
**    Date:  03/02/00
**    
**    File:  cur_trans.cc
**
*******************************************************************************
**
**    DESCRIPTION  curvelet transform program
**    ----------- 
**                 
******************************************************************************/

#include "IM_Obj.h"
#include "IM_IO.h"
#include "FFTN_2D.h"
#include "IM_Math.h"
#include "MR_Contrast.h"
#include "Curvelet.h"
#include "IM_Math.h"

char Name_Imag_In[256];  // input file image  
char Name_Imag_Out[256]; //  output file name  
char Name_Dir_Out[256]; //  output file name  

extern int  OptInd;
extern char *OptArg;

extern int  GetOpt(int argc, char **argv, char *opts);

Bool Verbose=False; 
int NbrScale2D = DEF_CUR_NBR_SCALE_2D;
int NbrScale1D = -1;
int BlockSize= DEF_CUR_BLOCK_SIZE;
type_ridgelet_WTtrans RidTrans = DEF_RID_TRANS;
Bool BlockOverlap = False;
Bool DirectionAnalysis = False;
Bool BorderSize=False;
Bool WriteStat = False;
Bool MadAngleNorm= False;
Bool AllStat = False;

/***************************************/

static void usage(char *argv[])
{
    // int i;
    fprintf(OUTMAN, "Usage: %s options in_image [out_file]\n\n", argv[0]);
    fprintf(OUTMAN, "   where options =  \n");

    fprintf(OUTMAN, "         [-n number_of_scales]\n");
    fprintf(OUTMAN, "             Number of scales used in the wavelet transform.\n");
    fprintf(OUTMAN, "             default is %d. \n", NbrScale2D);    
    manline();

    fprintf(OUTMAN, "         [-N number_of_scales]\n");
    fprintf(OUTMAN, "             Number of scales used in the ridgelet transform.\n");
    fprintf(OUTMAN, "             default is automatically calculated. \n");    
    manline();
    
    fprintf(OUTMAN, "         [-b BlockSize]\n");
    fprintf(OUTMAN, "             Block Size.\n");
    fprintf(OUTMAN, "             default is %d. \n", BlockSize);    
    manline();

    fprintf(OUTMAN, "         [-O]\n");
    fprintf(OUTMAN, "             Use overlapping block. Default is no. \n");    
    manline();
    
    fprintf(OUTMAN, "         [-D DirectFileName]\n");
    fprintf(OUTMAN, "             Directional Analysis File Name. \n");    
    manline();

    fprintf(OUTMAN, "         [-B]\n");
    fprintf(OUTMAN, "             Do not take into account coeffs on the borders. \n");    
    manline();
         
    vm_usage();
    manline();    
    verbose_usage();
    manline();         
    manline();
    exit(-1);
}
  
/*********************************************************************/

/* GET COMMAND LINE ARGUMENTS */
static void filtinit(int argc, char *argv[])
{
    int c;
#ifdef LARGE_BUFF
    int VMSSize=-1;
    Bool OptZ = False;
    char VMSName[1024] = "";
#endif     

    /* get options */
    while ((c = GetOpt(argc,argv,"SMBD:Ob:n:N:vzZ")) != -1) 
    {
	switch (c) 
        {    
	   case 'S': AllStat = True; break;
           case 'M': MadAngleNorm= True; break;
	   case 'B': BorderSize = True;break;
           case 'D': 	        
	        if (sscanf(OptArg,"%s",Name_Dir_Out) < 1)
		{
		   fprintf(OUTMAN, "Error: file name ... \n");
		   exit(-1);
		}
		DirectionAnalysis = True; 
		break;    
           case 'O': BlockOverlap= (BlockOverlap==True) ? False: True;break;
           case 'v': Verbose = True;break;
	   case 'b':
                if (sscanf(OptArg,"%d",&BlockSize) != 1) 
                {
                    fprintf(OUTMAN, "bad block size: %s\n", OptArg);
                    exit(-1);
                }
                break;
	   case 'n':
                /* -n <NbrScale2D> */
                if (sscanf(OptArg,"%d",&NbrScale2D) != 1) 
                {
                    fprintf(OUTMAN, "bad number of scales: %s\n", OptArg);
                    exit(-1);
                }
                if (NbrScale2D > MAX_SCALE)
                 {
                    fprintf(OUTMAN, "bad number of scales: %s\n", OptArg);
                    fprintf(OUTMAN, " Nbr Scales <= %d\n", MAX_SCALE);
                    exit(-1);
                }
                break;
	   case 'N':
                 if (sscanf(OptArg,"%d",&NbrScale1D) != 1) 
                {
                    fprintf(OUTMAN, "bad number of scales: %s\n", OptArg);
                    exit(-1);
                }
                break; 
#ifdef LARGE_BUFF
	    case 'z':
	        if (OptZ == True)
		{
                   fprintf(OUTMAN, "Error: Z option already set...\n");
                   exit(-1);
                }
	        OptZ = True;
	        break;
            case 'Z':
	        if (sscanf(OptArg,"%d:%s",&VMSSize, VMSName) < 1)
		{
		   fprintf(OUTMAN, "Error: syntaxe is Size:Directory ... \n");
		   exit(-1);
		}
	        if (OptZ == True)
		{
                   fprintf(OUTMAN, "Error: z option already set...\n");
                   exit(-1);
                }
		OptZ = True;
                break;
#endif
            case '?': usage(argv); break;
	    default: usage(argv); break;
 		}
	} 
      

       /* get optional input file names from trailing 
          parameters and open files */
       if (OptInd < argc) strcpy(Name_Imag_In, argv[OptInd++]);
         else usage(argv);
	 
	if (OptInd < argc)
	{
	   strcpy(Name_Imag_Out, argv[OptInd++]);
	   WriteStat = True;
	}
	
	/* make sure there are not too many parameters */
	if (OptInd < argc)
        {
		fprintf(OUTMAN, "Too many parameters: %s ...\n", argv[OptInd]);
		exit(-1);
	}  	
   
#ifdef LARGE_BUFF
    if (OptZ == True) vms_init(VMSSize, VMSName, Verbose);
#endif  
}
 
/*************************************************************************/
    
int main(int argc, char *argv[])
{
    int N,k,b;
    double Mean, Sigma, Skew, Curt;
    float  Min, Max;
    fitsstruct Header;
    char Cmd[512];
    Cmd[0] = '\0';
    for (k =0; k < argc; k++) sprintf(Cmd, "%s %s", Cmd, argv[k]);
     
    // Get command line arguments, open input file(s) if necessary
    lm_check(LIC_MR4);
    filtinit(argc, argv);

    if (Verbose == True)
    { 
        cout << endl << endl << "PARAMETERS: " << endl << endl;
        cout << "File Name in = " << Name_Imag_In << endl;
        cout << "File Name Out = " << Name_Imag_Out << endl;  
        // cout << "Ridgelet transform = " <<  StringRidTransform(RidTrans) << endl;  
        if (BlockOverlap == False) cout << "No overlapping " << endl;
        if (BlockSize > 0)  cout << "BlockSize = " << BlockSize <<endl;
        else  cout << "No partitionning  " << endl;
        cout << "Nbr Scale = " <<   NbrScale2D << endl; 
	if (NbrScale1D >= 0)
	 cout << "Nbr Scale 1D = " <<   NbrScale1D << endl;  
    }

    if (NbrScale2D < 1) 
    {
        cout << "Error: number of scales must be larger than 1 ... " << endl;
        exit(-1);
    }
    FilterAnaSynt SelectFilter(F_MALLAT_7_9);
    SubBandFilter SB1D(SelectFilter, NORM_L2);
    Curvelet Cur(SB1D);
    Cur.RidTrans = RidTrans;
    Cur.NbrScale2D = NbrScale2D;
    // NbrScale1D = 2;
    if (NbrScale1D >= 0)
    {
        Cur.NbrScale1D = NbrScale1D;
        Cur.GetAutoNbScale = False;
        if (NbrScale1D < 2) Cur.BlockOverlap = False;
        else Cur.BlockOverlap = BlockOverlap;
    }
    else Cur.BlockOverlap = BlockOverlap;
    // Cur.OddBlockSize=False;
    Cur.Verbose = Verbose; 
    
    Ifloat Data;
    fltarray Trans;
    io_read_ima_float(Name_Imag_In, Data, &Header);
    Header.origin = Cmd;
 
//    if ((BlockSize > 0) && (is_power_of_2(BlockSize) == False))
//    {
//           cout << "Error: Block size must be a power of two ... " << endl;
//           exit(-1);
//    }
    if (BlockSize > Data.nc())
    {
          cout << "Error: Block size must lower than the image size ... " << endl;
          exit(-1);
    }
    Cur.alloc(Data.nl(), Data.nc(), BlockSize, False);
    Cur.AngleNormalization=MadAngleNorm;
    // Cur.MSVST=True;
    Cur.transform(Data,Trans);
    int NbrBand = Cur.nbr_band();
    int NbrStatPerBand = 5; // moment of order 2,3,4  
    if (AllStat == True) NbrStatPerBand = 9;
    fltarray TabStat(NbrBand-1, NbrStatPerBand);
    fltarray Band;        
    if (Verbose == True) cout << "NbrBand = " <<  NbrBand << endl; 

    fltarray DirectionStat;
    intarray TabNbrDir(NbrBand);
    if (DirectionAnalysis == True)
    {
       for (b=0; b < NbrBand-1; b++) 
       {        
          int s2d, s1d;
          Cur.get_scale_number(b, s2d, s1d);
	  Ridgelet *Rid = Cur.get_ridgelet(s2d);
	  TabNbrDir(b) = Rid->rid_one_block_nl();
       }
       DirectionStat.alloc(NbrBand-1, TabNbrDir.max(), NbrStatPerBand+2);
    }
     
    for (b=0; b < NbrBand-1; b++) 
    {
        int s2d, s1d;
        Ifloat RidIma,IBand;
        Cur.get_scale_number(b, s2d, s1d);
	Ridgelet *Rid = Cur.get_ridgelet(s2d);
	float *PtrRidIma= Trans.buffer()+ s2d*Trans.ny()*Trans.nx();
	
	if (BorderSize == False) Cur.get_band(Trans, b, Band); 
 	else Cur.get_band_no_border(Trans,s2d,s1d,Band);

	float *PtrBuff = Band.buffer();
	N = Band.nx()*Band.ny();
        moment4(Band.buffer(), N, Mean, Sigma, Skew, Curt, Min, Max);
	 
        // im_moment4(Band, Mean, Sigma, Skew, Curt, Min, Max, BorderSize);
        TabStat(b, 0) = (float) Sigma;
	TabStat(b, 1) = (float) Skew;
	TabStat(b, 2) = (float) Curt;
        TabStat(b, 3) = (float) Min;
        TabStat(b, 4) = (float) Max; 
	if (Verbose == True)
            printf("  Band %d (%d,%d): Nl = %d, Nc = %d, Sigma = %5.3f, Skew = %5.3f, Curt = %5.3f, Min = %5.3f, Max = %5.3f\n",
	           b+1, s2d+1, s1d+1, Band.ny(), Band.nx(), TabStat(b,0), 
		   TabStat(b, 1),TabStat(b,2),TabStat(b, 3), TabStat(b, 4));
        
        if (AllStat == True) 
        {
	   float HC1, HC2, TC1, TC2;
	   if (b != NbrBand-1)  im_hc_test(Band, HC1, HC2, BorderSize, True);
           else im_hc_test(Band, HC1, HC2, BorderSize);
           TabStat(b, 5) = (float) HC1;
           TabStat(b, 6) = (float) HC2;
           gausstest(PtrBuff, N, TC1, TC2);
 	   TabStat(b, 7) = (float) TC1;
           TabStat(b, 8) = (float) TC2;
       	// float Norm = Cur.norm_band(s2d, s1d) * sqrt((float) Cur.TabBlockSize(s2d))
           if (Verbose == True)
            printf("   HC1 = %5.3f, HC2 = %5.3f, TC1 = %5.3f, TC2 = %5.3f\n",
 		   TabStat(b, 5), TabStat(b, 6), TabStat(b, 7), TabStat(b, 8));
        }
	
 	if (DirectionAnalysis == True)
	{	   
	    RidIma.alloc(PtrRidIma, Trans.ny(), Trans.nx());

	   // Rid->get_imablocksigma(RidIma, s1d, IBand);
	   int Ni = Rid->rid_one_block_nl();
 	   for (int i = 0; i < Ni; i++)
	   {
	      fltarray VectPix;
	      DirectionStat(b, i, 7) = Rid->get_angle_direction(i, True);
	      DirectionStat(b, i, 8) = Ni;
 	      Rid->get_anglevect(RidIma, s1d, i, VectPix);
	      float *Ptr = VectPix.buffer()+BorderSize;
 	      moment4(Ptr, VectPix.nx()-2*BorderSize, Mean, Sigma, Skew, Curt, Min, Max);
	      DirectionStat(b, i, 0) = (float) Sigma;
	      DirectionStat(b, i, 1) = (float) Skew;
	      DirectionStat(b, i, 2) = (float) Curt;
              DirectionStat(b, i, 3) = (float) Min;
              DirectionStat(b, i, 4) = (float) Max;         
	      
	      if (AllStat == True) 
              {
	         float HC1, HC2, TC1, TC2;
	         hc_test(VectPix.buffer(), VectPix.nx(), HC1, HC2, 0.);
		 gausstest(VectPix.buffer(), VectPix.nx(), TC1, TC2);
	         DirectionStat(b, i, 5) = (float) HC1;
                 DirectionStat(b, i, 6) = (float) HC2;
		 DirectionStat(b, i, 7) = (float) TC1;
                 DirectionStat(b, i, 8) = (float) TC2;
	      }
	   }
	}
    }
    if (WriteStat == True) fits_write_fltarr(Name_Imag_Out, TabStat); 
    if (DirectionAnalysis == True)  
        fits_write_fltarr(Name_Dir_Out, DirectionStat);
    exit(0);
}
