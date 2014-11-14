#include "a4.hpp"
#include "image.hpp"
#include "mastertempo.hpp"
#include <ctime>

void printProgBar( int percent ){
	std::string bar;

	for(int i = 0; i < 50; i++){
		if( i < (percent/2)){
			bar.replace(i,1,"=");
		}else if( i == (percent/2)){
			bar.replace(i,1,">");
		}else{
			bar.replace(i,1," ");
		}
	}

	std::cout<< "\r" "[" << bar << "] ";
	std::cout.width( 3 );
	std::cout<< percent << "%     " << std::flush;
}

#define FRAME_COUNT 3000

void a4_render(// What to render
	SceneNode* root,
               // Where to output the image
	const std::string& filename,
               // Image size
	int width, int height,
               // Viewing parameters
	const Point3D& eye, const Vector3D& view,
	const Vector3D& up, double fov,
               // Lighting parameters
	const Colour& ambient,
	const std::list<Light*>& lights
	)
{


	//MasterTempo masterTempo = MasterTempo("../data/1.mid");
	//masterTempo.printMidiEvents();
	//exit(1);



	int SSAAFactor = 1;
	height *= SSAAFactor;
	width *= SSAAFactor;

	double *rbuffer = (double*)malloc(sizeof(double)*3*width*height);
	int rbufferindex = 0;

////////
	const long double renderStartTime = time(0);
	long double previousFrameFinishTime = renderStartTime;

	for(int frame = 0; frame < FRAME_COUNT; frame++){
		rbufferindex=0;
////////


	for(int y = 0; y < height; y++){
		for(int x = 0; x < width; x++){
			double fovx = M_PI * ((double)fov/360.0);
			double fovy = (double)height/width * fovx;

			Point3D pixel = Point3D();
			pixel[0] = (2.0*x - width)/width * tan(fovx);
			pixel[1] = (-1 * (2.0*y - height)/height) * tan(fovy);
			pixel[2] = eye[2] - 1.0;

			Vector3D v = pixel - eye;

			v.normalize();
			bool rayWasRefracted = false;

			Intersection* col = root->intersect(pixel, v, Matrix4x4());
			Intersection* initHit = (Intersection*)malloc(sizeof(Intersection));
			*initHit = Intersection(col->getPoint(), col->getNormal(), col->getMaterial());
			initHit->setRefraction(col->isRefraction());
			initHit->setRefAngle(col->getRefAngle());

			while(col != NULL && col->isRefraction()){
				Point3D point = col->getPoint();
				Vector3D normal = col->getNormal();
				normal.normalize();
				Vector3D refAngle = col->getRefAngle();
				free(col);
				col = root->intersect(point, refAngle, Matrix4x4());
				rayWasRefracted = true;
			}

      //col->setPoint(initHit->getPoint());
      //col->setNormal(initHit->getNormal());
      //if(TEST != 0) exit(1);

			if(col == NULL){
				rbuffer[rbufferindex++] = 0;//-0.3 + (double)y/height;
				rbuffer[rbufferindex++] = 0;//0.4 + 0.2*(double)x/width;
				rbuffer[rbufferindex++] = 0;//0.6;
			} else {
				Material *mat = col->getMaterial();
				Colour kd = mat->getKD();

				Vector3D fc = Vector3D(0,0,0);//Vector3D(ambient.R()*kd.R(), ambient.G()*kd.G(), ambient.B()*kd.B());

				fc = shade(fc, lights, col, eye, root);
				fc.cap(1.0);

				if(rayWasRefracted){
					double opacityFactor = initHit->getNormal().dot(v);
					if(opacityFactor < 0) opacityFactor *= -1;

					if(opacityFactor > 1){
						std::cout << "opacityFactor = " << opacityFactor << "\n";
						exit(1);
					}

					opacityFactor = 1 - opacityFactor;
					opacityFactor = pow(opacityFactor, 3) + pow(opacityFactor + 0.1, 5);
					opacityFactor /= 2.0;
					if(opacityFactor > 1) opacityFactor = 1;
					//opacityFactor = 1 - opacityFactor;
					

					double transparancy = 0.9 * (1.0 - opacityFactor);//300.0 / glassTraversed;
					Colour glassKD = initHit->getMaterial()->getKD();
					Vector3D glassDiff = Vector3D(glassKD.R(), glassKD.G(), glassKD.B());
					Vector3D glassSpec = Vector3D(0.0, 0.0, 0.0);
					glassDiff = shade(glassDiff, lights, initHit, eye, root);

					//
					// Hacky way of calculating ONLY the specular part (use black diffuse and white spec)
					//
					Intersection glassSpecI = (*initHit);
					const Colour black = Colour(0.0, 0.0, 0.0);
					const Colour white = Colour(1.0, 1.0, 1.0);
					double shine = initHit->getMaterial()->getShininess();
					Material* onlySpec = (Material*) new PhongMaterial(black, white, shine);
					glassSpecI.setMaterial(onlySpec);
					glassSpec = shade(glassSpec, lights, &glassSpecI, eye, root);
					delete(onlySpec);
					//
					// </hack>
					//

					glassDiff.cap(1.0);
					fc = (transparancy*fc) + ((1.0-transparancy)*glassDiff) + glassSpec;

					// Sphere has radius 200
					//fc = (1.0 / (glassTraversed / 400.0)) * Vector3D(1.0, 1.0, 1.0);
				}

				fc.cap(1.0);

				rbuffer[rbufferindex++] = fc[0];
				rbuffer[rbufferindex++] = fc[1];
				rbuffer[rbufferindex++] = fc[2];
			}
			free(col);
			free(initHit);
		}
		printProgBar((y*100)/height);
	}

	//applySinCityFilter(rbuffer, height, width);

	long double delay = previousFrameFinishTime;
	previousFrameFinishTime = time(0);
	delay = previousFrameFinishTime - delay;

	std::cout << "  frame " << frame << " of " << FRAME_COUNT << " in " << delay << "s";


	Image img(width/SSAAFactor, height/SSAAFactor, 3);
	rbufferindex = 0;

	for (int y = 0; y < height/SSAAFactor; y++) {
		for (int x = 0; x < width/SSAAFactor; x++) {
			double red = 0.0;
			double green = 0.0;
			double blue = 0.0;
			for(int i = 0; i < SSAAFactor; i++){
				for(int j = 0; j < SSAAFactor; j++){
					int sx = SSAAFactor * x;
					int sy = SSAAFactor * y;
					double sa = SSAAFactor * SSAAFactor;
					int baseIndex = 3 * (sx + sy*width);
					red   += rbuffer[baseIndex   + 3*i + 3*width*j ] / sa;
					green += rbuffer[baseIndex+1 + 3*i + 3*width*j ] / sa;
					blue  += rbuffer[baseIndex+2 + 3*i + 3*width*j ] / sa;
				}
			}
			img(x, y, 0) = red;
			img(x, y, 1) = green;
			img(x, y, 2) = blue;
		}
	}
	img.savePng(filename);

//////
}
//////

}

void applySinCityFilter(double* rbuf, int height, int width){
	int bufIndex = 0;
	for(int h = 0; h < height; h++){
		for(int w = 0; w < width; w++){
			//std::cout << "BIDX " << bufIndex << "\n";
			Vector3D orig = Vector3D(rbuf[bufIndex], rbuf[bufIndex+1], rbuf[bufIndex+2]);
			orig.normalize();
			Vector3D final = orig;

			rbuf[bufIndex    ] = final[0];
			rbuf[bufIndex + 1] = final[1];
			rbuf[bufIndex + 2] = final[2];
			bufIndex += 3;
		}
	}
}


Vector3D shade(Vector3D fc, std::list<Light*> lights, Intersection* col, Point3D eye, SceneNode* root){
	Material *mat = col->getMaterial();
	Colour ks = mat->getKS();
	Colour kd = mat->getKD();
	int shininess = (int)mat->getShininess();
	
	Point3D point = col->getPoint();
	Vector3D normal = col->getNormal();
	normal.normalize();

	for (std::list<Light*>::iterator I = lights.begin(); I != lights.end(); ++I) {
		Point3D lpos = (*I)->position;

		// cast ray from point to light and see if it
		// intersects anything before the light (= shadow)
		double fp2 = 0.02;
		Point3D point2 = Point3D(point[0] + fp2*normal[0],
			point[1] + fp2*normal[1],
			point[2] + fp2*normal[2]);
		Vector3D pointToLight = lpos - point;
		pointToLight.normalize();
		Intersection* shadow = NULL;//root->intersect(point2, pointToLight, Matrix4x4());
		if(shadow == NULL){
			Colour lightColour = (*I)->colour;
			Vector3D lcol = Vector3D(lightColour.R(), lightColour.G(), lightColour.B());
	    	Vector3D iv = lpos - point; // incident vector
	    	Vector3D lpnorm = point - lpos;
	    	lpnorm.normalize();

	    	Vector3D tnorm = normal;
	    	tnorm.normalize();
	    	Vector3D s = lpos - point;
	    	s.normalize();
	    	Vector3D v = eye - point;
	    	v.normalize();
	    	// R = V - 2N(V . N)
	    	Vector3D r = iv - 2*(iv.dot(normal))*normal;
	    	r = -1 * r;
	    	r.normalize();
	    	double sdn = std::max(s.dot(tnorm), 0.0);
	    	Vector3D diffuse = sdn * Vector3D(lcol[0] * kd.R(),
	    		lcol[1] * kd.G(),
	    		lcol[2] * kd.B());
	    	Vector3D spec = Vector3D(0, 0, 0);
	    	if(shininess > 1){
		    	spec = std::pow(std::max(r.dot(v), 0.0), shininess) * Vector3D(ks.R() * lcol[0],
		    		ks.G() * lcol[1],
		    		ks.B() * lcol[2]);
	    	}

	    	// BLINN-PHONG SPEC
	    	Vector3D lightDirection = lpos - point;
	    	lightDirection.normalize();
	    	float cosAngInsidence = normal.dot(lightDirection);
	    	Vector3D blinnSpec = Vector3D(0,0,0);
	    	Vector3D specColour = Vector3D(ks.R()*lcol[0],ks.G()*lcol[1],ks.B()*lcol[2]);
	    	Vector3D halfAngle = (lpos - point) + (eye - point);
	    	Vector3D hacp = halfAngle;
	    	halfAngle.normalize();
	    	halfAngle = (1.0/hacp.normalize()) * halfAngle;
	    	halfAngle.normalize();
	    	double blinnTerm = halfAngle.dot(normal);
	    	if(blinnTerm < 0 || cosAngInsidence == 0.0) blinnTerm = 0;
	    	if(blinnTerm > 1) blinnTerm = 1;
	    	//std::cout << blinnTerm << "\n";
	    	blinnTerm = std::pow(blinnTerm, shininess);
	    	//std::cout << blinnTerm << "\n";
	    	blinnSpec = blinnTerm * specColour;


	    	fc = fc + diffuse + blinnSpec;
	    	//fc = Vector3D(blinnSpec[0], spec[0], 0);
	    } else {
	    	// the light doesn't contribute to the surface
	    	free(shadow);
	    }
	}
	return fc;
}
