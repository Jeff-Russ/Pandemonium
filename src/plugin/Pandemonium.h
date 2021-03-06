#include "ofxAudioPlugin.h"

#include "ofxAudioPlugin.h"
#include <fstream>


#include "ZGPlugin.h"
#include "ZGAUUtils.h"
#define EXTRA_MENU_ITEMS	3
#define ABOUT				1
#define REFRESH				0
#define SPACER				2

class Pandemonium: public ofxAudioPlugin, public ZGPlugin {
public:
	
	Pandemonium(ofxAudioPluginWrapper *wrapper): ZGPlugin(), ofxAudioPlugin(wrapper) {
		
		//ofstream out("/Users/marek/Desktop/Pandemonium Log.txt");
		//out << "Pandemonium" << endl;
		//out.close();
	
		loadPatch(0);

	}
	
	~Pandemonium() {
		pd->close();
	}
	
	int getNumParameters() {
		return params.size()+1;
	}
	
	// populate the incoming parameter info here about each of your parameters
	void getParameterInfo(int parameterId, ofxAudioPluginParameterInfo *info) {
		if(parameterId==0) {
			info->name = "Patch";
			info->min = 0;
			info->max = EXTRA_MENU_ITEMS + patchPaths.size()-1;
			info->type = ofxAudioPlugin_INDEXED;
			info->initial = 0;
		} else if(parameterId-1<params.size()) {
			ZGParameter * p = params.getParameter(parameterId-1);
			info->name = p->name;
			info->min = p->min;
			info->max = p->max;
			info->initial = p->value;
			if(p->type=="int" || p->type=="intslider") {
				info->type = ofxAudioPlugin_INT;
			} else {
				info->type = ofxAudioPlugin_FLOAT;
			}
		}
	}
	
	// if using an indexed parameter, set the name here
	string getParameterValueString(int parameterId, int index) {
		if(parameterId==0) {
			if(index==ABOUT) return "About Pandemonium...";
			if(index==REFRESH) return "Refresh...";
			if(index==SPACER) return "";
			return patchNameFromPath(patchPaths[index-EXTRA_MENU_ITEMS]);
		}
		return "";
	}
	

	float input[16384];
	float output[16384];

	void noteIn(int noteNum, int velocity) {
		pd->sendMidiNote(noteNum, velocity);
	}
	
	void ctlIn(int ctlNum, int value) {
		pd->sendMidiControlChange(ctlNum, value);
	}
	void bendIn(int value) {
		pd->sendMidiBend(value);
	}
	
	
	//program change
	void pgmIn(int value) {pd->sendMidiProgramChange(value);}
	
	// aftertouch
	void touchIn(int value) {pd->sendMidiAfterTouch(value);}
	
	void polyTouchIn(int note, int value) {pd->sendMidiPolyTouch(note, value);}
	
	
	
	
	void process(int numIns, float **ins, int numOuts, float **outs, int numFrames) {
		//printf("Num frames: %d\n", numFrames );
		if(!running) return;
		
		int menuIndex = getParameter(0);
		int patchIndex = menuIndex - EXTRA_MENU_ITEMS;
			takeItInTurnsMutex.lock();
		
		
		if(currPatchIndex!=patchIndex) {
			if(patchIndex>=0) {
				loadPatch(patchIndex);
				return;
			} else {
				if(menuIndex==ABOUT) {
					system("open http://www.mazbox.com/pandemonium/");
					setParameter(0, currPatchIndex+EXTRA_MENU_ITEMS);
				} else if(menuIndex==REFRESH) {
					findPatches();
					parametersChanged();
				}
				patchIndex = currPatchIndex;
				if(patchIndex>=patchPaths.size()) {
					patchIndex = EXTRA_MENU_ITEMS + patchPaths.size()-1;
				}
				setParameter(0, patchIndex+EXTRA_MENU_ITEMS);
			}
		} else {
			for(int i = 0; i < params.size(); i++) {		
				//printf("pandemonium: %x    ", (int)this);
				float val = getParameter(i+1);
				
				ZGParameter *p = params.getParameter(i);
				if(val!=p->value) {
					pd->sendFloat(p->name, val);
				}
			}
		}
		
		if(numOuts==2) {
			
			if(numIns==2) {
				// interleave samples
				for(int i = 0; i < numFrames; i++) {
					input[i*2] = ins[0][i];
					input[i*2+1] = ins[1][i];
				}
				
				pd->process(input, output, numFrames);
			} else if(numIns==1) { // 1 in, 2 outs
				pd->process(ins[0], output, numFrames);
			} else if(numIns==0) {// no inputs
				pd->process(input, output, numFrames);
			}
			
			bool foundNan = false;
			bool foundLoud = false;
			for(int i = 0; i < numFrames; i++) {

				outs[0][i] = output[i*2];
				outs[1][i] = output[i*2+1];
				if(outs[0][i]!=outs[0][i]) {
					foundNan = true;
				}
				else if(outs[0][i]>3) foundLoud = true;
			}
			if(foundNan) printf("Found nan\n");
			if(foundLoud) printf("Found loud\n");
		} else if(numIns==1 && numOuts==1) {
			pd->process(ins[0], outs[0], numFrames);
		} else if(numIns==0 && numOuts==1) {
			pd->process(input, outs[0], numFrames);
		}	
		takeItInTurnsMutex.unlock();
		lastBufferSize = numFrames;
	}
	
	
private:
	void loadPatch(int patchIndex) {

		if(patchPaths.size()>0) {
			params.load((char*)patchPaths[patchIndex].c_str());

			loadPatchFromDir(patchPaths[patchIndex]);
			
			setNumParameters(1+params.size());
		
			setParameter(0, patchIndex+EXTRA_MENU_ITEMS);
			for(int i = 0; i < params.size(); i++) {
				setParameter(i+1, params.getParameter(i)->value);
			}
			
			
			parametersChanged();
			
			
			currPatchIndex = patchIndex;
		}
	}
		static Mutex takeItInTurnsMutex;
};