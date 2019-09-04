/* User interface help strings
 * - Contains user interface strings displayed in the UI
 */

// TODO: Handle language selection somehow
enum LANGUAGES { en, NUM_LANGUAGES };

// ----- HELP STRINGS -----
// Help strings are displayed in their own window as a detailed explanation
// coupled with a teaspoon of historical content to give the text its context.

// TODO: Adopt these conventions in main.c
// ----- CONSTRUCTIONS -----
const char *aqueduct_help_str[NUM_LANGUAGES] = {"Provides water to baths and foundations."};
const char *basilica_help_str[NUM_LANGUAGES] = {""};
const char *farm_help_str[NUM_LANGUAGES] = {"Produces food."};
const char *senate_house_help_str[NUM_LANGUAGES] = {"Meeting place of the Senate. Enables Laws and increase political power."};
const char *temple_help_str[NUM_LANGUAGES] = {"Increases various powers."};
const char *coin_mint_help_str[NUM_LANGUAGES] = {"Reduces the negative effects of war. Increases gold income. Provides coinage."};
const char *insula_help_str[NUM_LANGUAGES] = {"Multistory apartment building for the plebians."};
const char *forum_help_str[NUM_LANGUAGES] = {"Public square."};
const char *port_ostia_help_str[NUM_LANGUAGES] = {"Enables import and export of foodstuffs"};
const char *bakery_help_str[NUM_LANGUAGES] = {"Increase the amount of food produces by farms."};
const char *circus_maximus_help_str[NUM_LANGUAGES] = {""};
const char *villa_publica_help_str[NUM_LANGUAGES] = {""};
const char *bath_help_str[NUM_LANGUAGES] = {""};

// ----- DESCRIPTION STRINGS -----
const char *bath_description_strs[NUM_LANGUAGES] = {
    "Roman bath complexes was a crucial part of life in the city"};
