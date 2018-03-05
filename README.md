# AudioLibraryRosettaStone

Maximally simple, mutually equivalent example code for low level (platform) audio APIs.

Generates a sine tone at a specific frequency, targeted at the stereo (left-right or frontleft-frontright) output channels.

# About

I did not do extensive testing. This code does not deal with out-of-memory errors, device hopping, format negotiation, or deinitialization. It's completely bare-bones.

Find a mistake? Please open an issue. Even if I don't fix it, other people will see the number of open issues, look at it, and maybe find the solution to a problem they have.

Want to add another backend? Make a "contrib" directory, put an example in it, and send a pull request. If I accept the pull request, and the example does not have its own license indication, you agree that the example code is licensed under the CC0 1.0 license, as explained below.

Disclaimer: I probably won't accept examples for very high level audio libraries, such as FMOD or Audiere for example.

# License

You can use this code as though it were public domain. Released under the Creative Commons "Zero" license 1.0, a public domain attribution: https://creativecommons.org/publicdomain/zero/1.0/

The CC0 license is an accepted Free Software license: https://www.gnu.org/licenses/license-list.html#CC0
