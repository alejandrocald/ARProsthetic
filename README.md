# ARProsthetic
Augmented Reality Prosthetic

Using the Unreal Engine 4 integrated with openCV, ARProsthetic is a prototype solution which is meant to demonstrate the attachment of a prosthetic arm model onto the user within augmented reality. Using only color detection and a simple printable colored arm band, the prosthetic arm tracks the arm of the user and gives an idea of how the prosthetic arm will look like on the user.

The code in this repository, namely WebCamReader.cpp and WebCamReader.h, are all the code in the c++ environment that is needed to run within UE4, however due to the large size of the UE4 editor, the rest of the project could not be placed within this repository. The only changes made to the UE4 editor were the openCV library and a blueprint which served as the models that held both the prosthetic arm and a plane which was assigned the material that the camera captures. Asside from these two things, a default UE4 project can run the WebCamReader class.
