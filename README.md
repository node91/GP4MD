GP4MD

===========================

This is a (test version) DLL that can:

\- Edit Magic Data of configured tracks

\- Extract Magic Data of configured tracks

\- Enable a race format of modern F1 sprint races (33% full distance ≈ 100km)

\- Change tyre wear and fuel consumption rates (for accelerated races)

\- Tweak CC cars behaviour



Information

===========================

\- Extract GP4MD anywhere you like and add GP4MD.dll to GP4 Memory Access

\- Keep the DLL and all INIs in the same folder

\- Possibly also works with CSM InjectDLL

\- Tested with GP4 Memory Access and some custom tracks

\- Best to use it with GPxPatch and GPxTrack enabled

\- In-game race distance must always be set to 100%

\- Best to use it with non-championship race

\- The code will first check if the .dat of the track has Magic Data or Laps, then fallback to GP4 default Magic Data and Laps, then overwrite the values with the INIs

\- Using faster fuel consumption creates higher engine temps as a side effect. Using multiplier 3.00 should still be ok. Not tested with custom physics

\- GP4MD.ini for General settings and Race Settings

\- Track INIs to override Lap settings and Magic Data for each track

\- Please read the descriptions in GP4MD.ini for more details and help

\- Leaving a certain key or entry blank in an INI will revert to default values

\- The Magic Data bump table is not editable or extractable

\- The GP4 amount of laps for some default 2001 tracks are wrong. These are written in the comments in the track INIs

\- I assume it should work with CSM and would allow to create a "Sprint Race" or "Full Race" setting in the CSM UI



