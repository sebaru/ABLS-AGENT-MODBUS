/******************************************************************************************************************************/
/* ABLS-AGENT-MODBUS  Gestion des modules MODBUS                                                                              */
/* Projet Abls-Habitat                   Gestion d'habitat                                     jeu. 24 déc. 2009 12:59:27 CET */
/* Auteur: LEFEVRE Sebastien                                                                                                  */
/******************************************************************************************************************************/
/*
 * Modbus.c
 * This file is part of Abls-Habitat
 *
 * Copyright (C) 1988-2026 - Sébastien LEFÈVRE
 *
 * ABLS-AGENT-MODBUS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ABLS-AGENT-MODBUS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ABLS-AGENT-MODBUS; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

 #include <stdio.h>
 #include <fcntl.h>
 #include <sys/types.h>
 #include <sys/time.h>
 #include <sys/stat.h>
 #include <errno.h>
 #include <sys/prctl.h>
 #include <termios.h>
 #include <unistd.h>
 #include <string.h>
 #include <stdlib.h>
 #include <signal.h>
 #include <semaphore.h>
 #include <netinet/in.h>
 #include <netdb.h>
 #include <time.h>

 #include "modbus.h"

static guint Modbus_top_ds(void)
 { struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (guint)(ts.tv_sec * 10 + ts.tv_nsec / 100000000);
 }

/******************************************************************************************************************************/
/* Modbus_SET_DO: Met a jour une sortie TOR en fonction du jsonnode en parametre                                              */
/* Entrée: le agent et le buffer Josn                                                                                        */
/* Sortie: Niet                                                                                                               */
/******************************************************************************************************************************/
 static void Modbus_SET_DO ( struct ABLS_AGENT *agent, JsonNode *msg )
  { struct MODBUS_VARS *vars = agent->vars;
    gchar *msg_agent_tech_id  = Json_get_string ( msg, "mqtt_topic_lvl1" );
    gchar *msg_agent_acronyme = Json_get_string ( msg, "mqtt_topic_lvl2" );
    gchar *msg_tech_id        = Json_get_string ( msg, "tech_id" );
    gchar *msg_acronyme       = Json_get_string ( msg, "acronyme" );

    if (!msg_agent_tech_id)
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_ERR, "requete mal formée manque msg_agent_tech_id" ); }
    else if (!msg_agent_acronyme)
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_ERR, "requete mal formée manque msg_agent_acronyme" ); }
    else if (strcasecmp (msg_agent_tech_id, agent->agent_tech_id))
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_DEBUG, "Pas pour nous" ); }
    else if (!Json_has_member ( msg, "etat" ))
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_ERR, "Requete mal formée manque etat" ); }
    else
     { gboolean etat = Json_get_bool ( msg, "etat" );
       for (gint num=0; num<vars->nbr_sortie_tor; num++)
        { if ( vars->DO && vars->DO[num] &&
               !strcasecmp ( Json_get_string(vars->DO[num], "agent_acronyme"), msg_agent_acronyme ) )
           { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_INFO, "SET_DO '%s:%s'/'%s:%s'=%d",
                       msg_agent_tech_id, msg_agent_acronyme, msg_tech_id, msg_acronyme, etat );
             Json_add_bool ( vars->DO[num], "etat", etat );
             break;
           }
        }
     }
  }
/******************************************************************************************************************************/
/* Modbus_SET_AO: Met a jour une sortie ANA en fonction du jsonnode en parametre                                              */
/* Entrée: le agent et le buffer Josn                                                                                        */
/* Sortie: Niet                                                                                                               */
/******************************************************************************************************************************/
 static void Modbus_SET_AO ( struct ABLS_AGENT *agent, JsonNode *msg )
  { struct MODBUS_VARS *vars = agent->vars;
    gchar *msg_agent_tech_id  = Json_get_string ( msg, "mqtt_topic_lvl1" );
    gchar *msg_agent_acronyme = Json_get_string ( msg, "mqtt_topic_lvl2" );
    gchar *msg_tech_id        = Json_get_string ( msg, "tech_id" );
    gchar *msg_acronyme       = Json_get_string ( msg, "acronyme" );

    if (!msg_agent_tech_id)
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_ERR, "requete mal formée manque msg_agent_tech_id" ); }
    else if (!msg_agent_acronyme)
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_ERR, "requete mal formée manque msg_agent_acronyme" ); }
    else if (strcasecmp (msg_agent_tech_id, agent->agent_tech_id))
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_DEBUG, "Pas pour nous" ); }
    else if (!Json_has_member ( msg, "valeur" ))
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_ERR, "Requete mal formée manque etat" ); }
    else
     { gdouble valeur = Json_get_double ( msg, "valeur" );
       for (gint num=0; num<vars->nbr_sortie_ana; num++)
        { if ( vars->AO && vars->AO[num] &&
               !strcasecmp ( Json_get_string(vars->AO[num], "thread_acronyme"), msg_agent_acronyme ) )
           { gint type_borne = Json_get_int    ( vars->AO[num], "type_borne" );
             gint new_val_int;
             switch( type_borne )
              { case WAGO_750550: if (valeur > 10.0) valeur = 10.0;                                       /* Borne WAGO 0-10V */
                                  if (valeur <  0.0) valeur = 0.0;
                                  new_val_int = (gint) (32767.0 * valeur / 10.0);        /* Borne sur 32768 valeurs de sortie */
                                  break;
/*              case WAGO_XXX   : gdouble min     = Json_get_double ( vars->AO[num], "min" );
                                  gdouble max     = Json_get_double ( vars->AO[num], "max" );
                                  if (valeur < min) valeur = min;
                                  if (valeur > max) valeur = max;
                                  new_val_int = (gint) (4095 * (valeur - min) / max);
                                  break;*/
                default: new_val_int = 0;
              }
             Json_add_int ( vars->AO[num], "val_int", new_val_int );
             Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_NOTICE, "SET_AO '%s:%s'/'%s:%s'=%f (val_int=%d)",
                       msg_agent_tech_id, msg_agent_acronyme, msg_tech_id, msg_acronyme, valeur, new_val_int );
             break;
           }
        }
     }
  }
/******************************************************************************************************************************/
/* Modbus_Sync_INPUT_to_master: Synchronise les Output du master vers le wago                                                 */
/* Entrée: le agent                                                                                                          */
/* Sortie: Niet                                                                                                               */
/******************************************************************************************************************************/
 static void Modbus_Sync_INPUT_to_master ( struct ABLS_AGENT *agent )
  { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_INFO, "Syncing IO to master" );
    struct MODBUS_VARS *vars = agent->vars;

    for ( gint cpt = 0; cpt<vars->nbr_entree_tor; cpt++)
     { if (vars->DI[cpt]) Json_add_bool ( vars->DI[cpt], "need_sync", TRUE ); }

    for ( gint cpt = 0; cpt<vars->nbr_entree_ana; cpt++)
     { if (vars->AI[cpt]) Json_add_bool ( vars->AI[cpt], "need_sync", TRUE ); }
  }
/******************************************************************************************************************************/
/* Deconnecter: Deconnexion du agent                                                                                         */
/* Entrée: un id                                                                                                              */
/* Sortie: néant                                                                                                              */
/******************************************************************************************************************************/
 static void Deconnecter_module ( struct ABLS_AGENT *agent )
  { struct MODBUS_VARS *vars = agent->vars;
    if (!agent) return;
    if (vars->started == FALSE) return;

    gchar *hostname       = Json_get_string ( agent->api_config, "hostname" );

    close ( vars->connexion );
    vars->connexion = 0;
    vars->started = FALSE;
    vars->request = FALSE;
    vars->nbr_deconnect++;
    vars->date_retente = Modbus_top_ds() + MODBUS_RETRY;
    if (vars->DI) { g_free(vars->DI); vars->DI = NULL; }
    if (vars->DO) { g_free(vars->DO); vars->DO = NULL; }
    if (vars->AI) { g_free(vars->AI); vars->AI = NULL; }
    if (vars->AO) { g_free(vars->AO); vars->AO = NULL; }
    vars->nbr_entree_tor = 0;
    vars->nbr_entree_ana = 0;
    vars->nbr_sortie_ana = 0;
    vars->nbr_sortie_tor = 0;
    Agent_send_comm_to_master ( agent, FALSE );
    Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_INFO, "Module '%s' disconnected", hostname );
  }
/******************************************************************************************************************************/
/* Connecter: Tentative de connexion au serveur                                                                               */
/* Entrée: une nom et un password                                                                                             */
/* Sortie: les variables globales sont initialisées, FALSE si pb                                                              */
/******************************************************************************************************************************/
 static gboolean Connecter_module ( struct ABLS_AGENT *agent )
  { struct MODBUS_VARS *vars = agent->vars;
    struct addrinfo *result, *rp;
    struct timeval sndtimeout;
    struct addrinfo hints;
    gint connexion = 0, s;

    gchar *hostname       = Json_get_string ( agent->api_config, "hostname" );

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */

    sndtimeout.tv_sec  = 10;
    sndtimeout.tv_usec =  0;

    Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_DEBUG, "Trying to connect agent to '%s'", hostname );

    s = getaddrinfo( hostname, "502", &hints, &result);
    if (s != 0)
    { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_ERR,
                "getaddrinfo Failed for agent %s (%s)", hostname, gai_strerror(s) );
       return(FALSE);
     }

   /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully connect(2).
       If socket(2) (or connect(2)) fails, we (close the socket
       and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next)
     { connexion = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
       if (connexion == -1)
        { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_ERR,
                   "Socket creation failed for modbus '%s'", hostname );
          continue;
        }

       if ( setsockopt ( connexion, SOL_SOCKET, SO_SNDTIMEO, (char *)&sndtimeout, sizeof(sndtimeout)) < 0 )
        { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_ERR,
                   "Socket Set Options failed for modbus '%s'", hostname );
          continue;
        }

       if (connect(connexion, rp->ai_addr, rp->ai_addrlen) != -1)
        { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_INFO,
                   "Using family=%d for host '%s'", rp->ai_family, hostname );

          break;  /* Success */
        }
       else
        { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_ERR,
                   "'connexion refused by agent '%s' family=%d error '%s'",
                   hostname, rp->ai_family, strerror(errno) );
        }
       close(connexion);                                                       /* Suppression de la socket qui n'a pu aboutir */
     }
    freeaddrinfo(result);
    if (rp == NULL) return(FALSE);                                                                     /* Erreur de connexion */

    fcntl( connexion, F_SETFL, SO_KEEPALIVE | SO_REUSEADDR );
    vars->connexion = connexion;                                                                          /* Sauvegarde du fd */
    vars->date_last_reponse = Modbus_top_ds();
    vars->date_retente   = 0;
    vars->transaction_id = 1;
    vars->started        = TRUE;
    vars->mode           = MODBUS_GET_DESCRIPTION;
    Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_NOTICE, "Module Connected" );

    return(TRUE);
  }
/******************************************************************************************************************************/
/* Interroger_description : envoie une commande d'identification au agent                                                    */
/* Entrée: L'id de la transmission, et la trame a transmettre                                                                 */
/******************************************************************************************************************************/
 static void Interroger_description( struct ABLS_AGENT *agent )
  { struct MODBUS_VARS *vars = agent->vars;
    struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */

    gchar *hostname       = Json_get_string ( agent->api_config, "hostname" );

    vars->transaction_id++;
    requete.transaction_id = htons(vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    requete.taille         = htons( 0x006 );                                                /* taille, en comptant le unit_id */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_READ_REGISTER;
    requete.adresse        = htons( 0x2020 );
    requete.nbr            = htons( 16 );

    gint retour = write ( vars->connexion, &requete, 12 );
    if ( retour != 12 )                                                                                /* Envoi de la requete */
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_WARNING,
               "Failed for agent '%s': error %d/%s", hostname, retour, strerror(errno) );
       Deconnecter_module( agent );
     }
    else
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_DEBUG, "OK" );
       vars->request = TRUE;                                                                      /* Une requete a élé lancée */
     }
  }
/******************************************************************************************************************************/
/* Interroger_description : envoie une commande d'identification au agent                                                    */
/* Entrée: L'id de la transmission, et la trame a transmettre                                                                 */
/******************************************************************************************************************************/
 static void Interroger_firmware( struct ABLS_AGENT *agent )
  { struct MODBUS_VARS *vars = agent->vars;
    struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */

    gchar *hostname       = Json_get_string ( agent->api_config, "hostname" );

    vars->transaction_id++;
    requete.transaction_id = htons(vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    requete.taille         = htons( 0x006 );                                                /* taille, en comptant le unit_id */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_READ_REGISTER;
    requete.adresse        = htons( 0x2023 );
    requete.nbr            = htons( 16 );

    gint retour = write ( vars->connexion, &requete, 12 );
    if ( retour != 12 )                                                                                /* Envoi de la requete */
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_WARNING,
               "Failed for agent '%s': error %d/%s", hostname, retour, strerror(errno) );
       Deconnecter_module( agent );
     }
    else
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_DEBUG, "OK" );
       vars->request = TRUE;                                                                    /* Une requete a élé lancée */
     }
  }
/******************************************************************************************************************************/
/* Interroger_borne: Interrogation d'une borne du agent                                                                      */
/* Entrée: identifiants des modules et borne                                                                                  */
/* Sortie: néant                                                                                                              */
/******************************************************************************************************************************/
 static void Init_watchdog1( struct ABLS_AGENT *agent )
  { struct MODBUS_VARS *vars = agent->vars;
    struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */

    gchar *hostname       = Json_get_string ( agent->api_config, "hostname" );

    vars->transaction_id++;
    requete.transaction_id = htons(vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    requete.taille         = htons( 0x0006 );                                               /* taille, en comptant le unit_id */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_WRITE_REGISTER;
    requete.adresse        = htons( 0x100A );                                                                   /* Stop Timer */
    requete.valeur         = htons( 0x0000 );

    gint retour = write ( vars->connexion, &requete, 12 );
    if ( retour != 12 )                                                                                /* Envoi de la requete */
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_WARNING,
               "Failed for agent '%s': error %d/%s", hostname, retour, strerror(errno) );
       Deconnecter_module( agent );
     }
    else
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_DEBUG, "OK" );
       vars->request = TRUE;                                                                    /* Une requete a élé lancée */
     }
  }
/******************************************************************************************************************************/
/* Interroger_borne: Interrogation d'une borne du agent                                                                      */
/* Entrée: identifiants des modules et borne                                                                                  */
/* Sortie: ?                                                                                                                  */
/******************************************************************************************************************************/
 static void Init_watchdog2( struct ABLS_AGENT *agent )
  { struct MODBUS_VARS *vars = agent->vars;
    struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */

    gchar *hostname       = Json_get_string ( agent->api_config, "hostname" );

    vars->transaction_id++;
    requete.transaction_id = htons(vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    requete.taille         = htons( 0x0006 );                                               /* taille, en comptant le unit_id */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_WRITE_REGISTER;
    requete.adresse        = htons( 0x1009 );                                   /* Close MODBUS socket after watchdog timeout */
    requete.valeur         = htons( 0x0001 );

    gint retour = write ( vars->connexion, &requete, 12 );
    if ( retour != 12 )                                                                                /* Envoi de la requete */
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_WARNING,
               "Failed for agent '%s': error %d/%s", hostname, retour, strerror(errno) );
       Deconnecter_module( agent );
     }
    else
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_DEBUG, "OK" );
       vars->request = TRUE;                                                /* Une requete a élé lancée */
     }
  }
/******************************************************************************************************************************/
/* Interroger_borne: Interrogation d'une borne du agent                                                                      */
/* Entrée: identifiants des modules et borne                                                                                  */
/* Sortie: This register stores the watchdog timeout value as an unsigned 16 bit value. The Description default value is 0.   */
/* Setting this value will not trigger the watchdog. However, a non zero value must be stored in this register before the     */
/* watchdog can be triggered. The time value is stored in multiples of 100ms (e.g., 0x0009 is .9 seconds). It is not possible */
/* to modify this value while the watchdog is running                                                                         */
/******************************************************************************************************************************/
 static void Init_watchdog3( struct ABLS_AGENT *agent )
  { struct MODBUS_VARS *vars = agent->vars;
    struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */

    gchar *hostname       = Json_get_string ( agent->api_config, "hostname" );

    vars->transaction_id++;
    requete.transaction_id = htons(vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    requete.taille         = htons( 0x0006 );                                               /* taille, en comptant le unit_id */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_WRITE_REGISTER;
    requete.adresse        = htons( 0x1000 );                                                       /* Watchdog Time register */
    requete.valeur         = htons( Json_get_int ( agent->api_config, "watchdog" ) ); /* coupure sortie, en 100ième de secondes  */

    gint retour = write ( vars->connexion, &requete, 12 );
    if ( retour != 12 )                                                                                /* Envoi de la requete */
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_WARNING,
               "Failed for agent '%s': error %d/%s", hostname, retour, strerror(errno) );
       Deconnecter_module( agent );
     }
    else
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_DEBUG, "OK" );
       vars->request = TRUE;                                                /* Une requete a élé lancée */
     }
  }
/******************************************************************************************************************************/
/* Interroger_borne: Interrogation d'une borne du agent                                                                      */
/* Entrée: identifiants des modules et borne                                                                                  */
/* Sortie: ?                                                                                                                  */
/******************************************************************************************************************************/
 static void Init_watchdog4( struct ABLS_AGENT *agent )
  { struct MODBUS_VARS *vars = agent->vars;
    struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */

    gchar *hostname       = Json_get_string ( agent->api_config, "hostname" );

    vars->transaction_id++;
    requete.transaction_id = htons(vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    requete.taille         = htons( 0x0006 );                                               /* taille, en comptant le unit_id */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_WRITE_REGISTER;
    requete.adresse        = htons( 0x100A );
    requete.valeur         = htons( 0x0001 );                                                                  /* Start Timer */

    gint retour = write ( vars->connexion, &requete, 12 );
    if ( retour != 12 )                                                                                /* Envoi de la requete */
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_WARNING,
                "Failed for agent '%s': error %d/%s", hostname, retour, strerror(errno) );
       Deconnecter_module( agent );
     }
    else
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_DEBUG, "OK" );
       vars->request = TRUE;                                                /* Une requete a élé lancée */
     }
  }
/******************************************************************************************************************************/
/* Interroger_nbr_entree_ANA : Demander au agent d'envoyer son nombre d'entree ANALOGIQUE                                    */
/* Entrée: L'id de la transmission, et la trame a transmettre                                                                 */
/******************************************************************************************************************************/
 static void Interroger_nbr_entree_ANA( struct ABLS_AGENT *agent )
  { struct MODBUS_VARS *vars = agent->vars;
    struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */

    gchar *hostname       = Json_get_string ( agent->api_config, "hostname" );

    vars->transaction_id++;
    requete.transaction_id = htons(vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    requete.taille         = htons( 0x006 );                                                /* taille, en comptant le unit_id */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_READ_REGISTER;
    requete.adresse        = htons( 0x1023 );
    requete.nbr            = htons( 0x0001 );

    gint retour = write ( vars->connexion, &requete, 12 );
    if ( retour != 12 )                                                                                /* Envoi de la requete */
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_WARNING,
               "Failed for agent '%s': error %d/%s", hostname, retour, strerror(errno) );
       Deconnecter_module( agent );
     }
    else
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_DEBUG, "OK" );
       vars->request = TRUE;                                                                    /* Une requete a élé lancée */
     }
  }
/******************************************************************************************************************************/
/* Interroger_nbr_entree_ANA : Demander au agent d'envoyer son nombre de sortie ANALOGIQUE                                   */
/* Entrée: L'id de la transmission, et la trame a transmettre                                                                 */
/******************************************************************************************************************************/
 static void Interroger_nbr_sortie_ANA( struct ABLS_AGENT *agent )
  { struct MODBUS_VARS *vars = agent->vars;
    struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */

    gchar *hostname       = Json_get_string ( agent->api_config, "hostname" );

    vars->transaction_id++;
    requete.transaction_id = htons(vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    requete.taille         = htons( 0x006 );                                                /* taille, en comptant le unit_id */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_READ_REGISTER;
    requete.adresse        = htons( 0x1022 );
    requete.nbr            = htons( 0x0001 );

    gint retour = write ( vars->connexion, &requete, 12 );
    if ( retour != 12 )                                                                                /* Envoi de la requete */
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_WARNING,
               "Failed for agent '%s': error %d/%s", hostname, retour, strerror(errno) );
       Deconnecter_module( agent );
     }
    else
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_DEBUG, "OK" );
       vars->request = TRUE;                                                                    /* Une requete a élé lancée */
     }
  }
/******************************************************************************************************************************/
/* Interroger_nbr_entree_TOR : Demander au agent d'envoyer son nombre d'entree TOR                                           */
/* Entrée: L'id de la transmission, et la trame a transmettre                                                                 */
/******************************************************************************************************************************/
 static void Interroger_nbr_entree_TOR( struct ABLS_AGENT *agent )
  { struct MODBUS_VARS *vars = agent->vars;
    struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */

    gchar *hostname       = Json_get_string ( agent->api_config, "hostname" );

    vars->transaction_id++;
    requete.transaction_id = htons(vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    requete.taille         = htons( 0x006 );                                                /* taille, en comptant le unit_id */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_READ_REGISTER;
    requete.adresse        = htons( 0x1025 );
    requete.nbr            = htons( 0x0001 );

    gint retour = write ( vars->connexion, &requete, 12 );
    if ( retour != 12 )                                                                                /* Envoi de la requete */
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_WARNING,
               "Failed for agent '%s': error %d/%s", hostname, retour, strerror(errno) );
       Deconnecter_module( agent );
     }
    else
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_DEBUG, "OK" );
       vars->request = TRUE;                                                                    /* Une requete a élé lancée */
     }
  }
/******************************************************************************************************************************/
/* Interroger_nbr_sortie_TOR : Demander au agent d'envoyer son nombre de sortie TOR                                          */
/* Entrée: L'id de la transmission, et la trame a transmettre                                                                 */
/******************************************************************************************************************************/
 static void Interroger_nbr_sortie_TOR( struct ABLS_AGENT *agent )
  { struct MODBUS_VARS *vars = agent->vars;
    struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */

    gchar *hostname       = Json_get_string ( agent->api_config, "hostname" );

    vars->transaction_id++;
    requete.transaction_id = htons(vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    requete.taille         = htons( 0x006 );                                                /* taille, en comptant le unit_id */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_READ_REGISTER;
    requete.adresse        = htons( 0x1024 );
    requete.nbr            = htons( 0x0001 );

    gint retour = write ( vars->connexion, &requete, 12 );
    if ( retour != 12 )                                                                                /* Envoi de la requete */
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_WARNING,
                 "Failed for agent '%s': error %d/%s", hostname, retour, strerror(errno) );
       Deconnecter_module( agent );
     }
    else
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_DEBUG, "OK" );
       vars->request = TRUE;                                                                      /* Une requete a élé lancée */
     }
  }
/******************************************************************************************************************************/
/* Interroger_borne: Interrogation d'une borne du agent                                                                      */
/* Entrée: identifiants des modules et borne                                                                                  */
/* Sortie: ?                                                                                                                  */
/******************************************************************************************************************************/
 static void Interroger_entree_tor( struct ABLS_AGENT *agent )
  { struct MODBUS_VARS *vars = agent->vars;
    struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */

    gchar *hostname       = Json_get_string ( agent->api_config, "hostname" );

    vars->transaction_id++;
    requete.transaction_id = htons(vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    requete.taille         = htons( 0x0006 );                                               /* taille, en comptant le unit_id */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_READ_COIL;
    requete.adresse        = 0x00;
    requete.nbr            = htons( vars->nbr_entree_tor );

    gint retour = write ( vars->connexion, &requete, 12 );
    if ( retour != 12 )                                                                                /* Envoi de la requete */
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_WARNING,
                 "Failed for agent '%s': error %d/%s", hostname, retour, strerror(errno) );
       Deconnecter_module( agent );
     }
    else { vars->request = TRUE; }                                                                /* Une requete a élé lancée */
  }
/******************************************************************************************************************************/
/* Interroger_entree_ana: Interrogation des entrees analogique d'un agent wago                                               */
/* Entrée: identifiants des modules et borne                                                                                  */
/* Sortie: ?                                                                                                                  */
/******************************************************************************************************************************/
 static void Interroger_entree_ana( struct ABLS_AGENT *agent )
  { struct MODBUS_VARS *vars = agent->vars;
    struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */

    gchar *hostname       = Json_get_string ( agent->api_config, "hostname" );

    vars->transaction_id++;
    requete.transaction_id = htons(vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    requete.taille         = htons( 0x0006 );                                               /* taille, en comptant le unit_id */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_READ_REGISTER;
    requete.adresse        = 0x00;
    requete.nbr            = htons( vars->nbr_entree_ana );

    gint retour = write ( vars->connexion, &requete, 12 );
    if ( retour != 12 )                                                                                /* Envoi de la requete */
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_WARNING,
                 "Failed for agent '%s': error %d/%s", hostname, retour, strerror(errno) );
       Deconnecter_module( agent );
     }
    else { vars->request = TRUE; }                                                                /* Une requete a élé lancée */
  }
/******************************************************************************************************************************/
/* Interroger_borne: Interrogation d'une borne du agent                                                                      */
/* Entrée: identifiants des modules et borne                                                                                  */
/* Sortie: ?                                                                                                                  */
/******************************************************************************************************************************/
 static void Interroger_sortie_tor( struct ABLS_AGENT *agent )
  { struct MODBUS_VARS *vars = agent->vars;
    struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */
    gint taille, nbr_data;

    gchar *hostname       = Json_get_string ( agent->api_config, "hostname" );

    memset(&requete, 0, sizeof(requete) );                                               /* Mise a zero globale de la requete */
    nbr_data = ((vars->nbr_sortie_tor-1)/8)+1;
    vars->transaction_id++;
    requete.transaction_id = htons(vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    taille                 = 0x0007 + nbr_data;
    requete.taille         = htons( taille );                                                                       /* taille */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_WRITE_MULTIPLE_COIL;
    requete.adresse        = 0x00;
    requete.nbr            = htons( vars->nbr_sortie_tor );                                                    /* bit count */
    requete.data[2]        = nbr_data;                                                                          /* Byte count */

    if (vars->DO)
     { gint cpt_poid, cpt_byte, cpt;
       for ( cpt_poid = 1, cpt_byte = 3, cpt = 0; cpt<vars->nbr_sortie_tor; cpt++ )
        { if (cpt_poid == 256) { cpt_byte++; cpt_poid = 1; }
          if ( vars->DO[cpt] )
           { if (Json_get_bool ( vars->DO[cpt], "etat" )) { requete.data[cpt_byte] |= cpt_poid; } }
          cpt_poid = cpt_poid << 1;
        }
     }

    gint retour = write ( vars->connexion, &requete, taille+6 );
    if ( retour != taille+6 )                                                                          /* Envoi de la requete */
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_WARNING,
             "Failed for agent '%s': error %d/%s", hostname, retour, strerror(errno) );
       Deconnecter_module( agent );
     }
    else { vars->request = TRUE; }                                                                /* Une requete a élé lancée */
  }
/******************************************************************************************************************************/
/* Interroger_sortie_ana: Envoie les informations liées aux sorties ANA du agent                                             */
/* Entrée: le agent à interroger                                                                                             */
/* Sortie: néant                                                                                                              */
/******************************************************************************************************************************/
 static void Interroger_sortie_ana( struct ABLS_AGENT *agent )
  { struct MODBUS_VARS *vars = agent->vars;
    struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */
    gint taille;

    gchar *hostname       = Json_get_string ( agent->api_config, "hostname" );

    memset(&requete, 0, sizeof(requete) );                                               /* Mise a zero globale de la requete */
    vars->transaction_id++;
    requete.transaction_id = htons(vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    taille                 = 0x0006 + (vars->nbr_sortie_ana*2 + 1);
    requete.taille         = htons( taille );                                                                       /* taille */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_WRITE_MULTIPLE_REGISTER;
    requete.adresse        = 0x00;
    requete.nbr            = htons( vars->nbr_sortie_ana );                                                      /* bit count */
    requete.data[2]        = (vars->nbr_sortie_ana*2);                                                          /* Byte count */

    if (vars->AO)
     { gint cpt_byte, cpt;
       for ( cpt_byte = 3, cpt = 0; cpt<vars->nbr_sortie_ana; cpt++)
        { if (vars->AO[cpt])
           { gint val_int = Json_get_int ( vars->AO[cpt], "val_int" );
             requete.data [cpt_byte  ] =  val_int >> 8;                                                /* Octet de poids fort */
             requete.data [cpt_byte+1] =  val_int & 0xFF;                                            /* Octet de poids faible */
             cpt_byte += 2;
           }
        }
     }

    gint retour = write ( vars->connexion, &requete, taille+6 );
    if ( retour != taille+6 )                                                                          /* Envoi de la requete */
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_WARNING,
                 "Failed for agent '%s': error %d/%s", hostname, retour, strerror(errno) );
       Deconnecter_module( agent );
     }
    else { vars->request = TRUE; }                                                                /* Une requete a élé lancée */
  }
/******************************************************************************************************************************/
/* Modbus_load_io_config : Charge les données des IO du agent                                                                */
/* Entrée : la structure referencant le agent                                                                                */
/* Sortie : rien                                                                                                              */
/******************************************************************************************************************************/
 static void Modbus_load_io_config ( struct ABLS_AGENT *agent )
  { struct MODBUS_VARS *vars = agent->vars;

/***************************************************** Mapping des AnalogInput ************************************************/
    Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_INFO, "Allocate %d AI", vars->nbr_entree_ana );
    if(vars->nbr_entree_ana)
     { vars->AI = g_try_malloc0( sizeof(JsonNode *) * vars->nbr_entree_ana );
       if (vars->AI)
        { JsonArray *array = Json_get_array ( agent->api_config, "AI" );
          for ( guint cpt = 0; cpt < json_array_get_length ( array ); cpt++ )
           { JsonNode *element = json_array_get_element ( array, cpt );
             gint num = Json_get_int ( element, "num" );
             if ( 0 <= num && num < vars->nbr_entree_ana )
              { vars->AI[num] = element;
                Json_add_double ( vars->AI[num], "valeur", 0.0 );
                Json_add_bool   ( vars->AI[num], "in_range", FALSE );
                Json_add_bool   ( vars->AI[num], "need_sync", TRUE );
                Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_NOTICE, "New AI '%s' (%s, %s)",
                          Json_get_string ( vars->AI[num], "thread_acronyme" ),
                          Json_get_string ( vars->AI[num], "libelle" ),
                          Json_get_string ( vars->AI[num], "unite" ) );
              } else Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_WARNING, "Map AI: num %d out of range '%d'",
                               num, vars->nbr_entree_ana );
           }
        }
       else Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_ALERT, "Memory Error for AI" );
     }
/***************************************************** Mapping des DigitalInput ***********************************************/
    Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_INFO, "Allocate %d DI", vars->nbr_entree_tor );
    if(vars->nbr_entree_tor)
     { vars->DI = g_try_malloc0( sizeof(JsonNode *) * vars->nbr_entree_tor );
       if (vars->DI)
        { JsonArray *array = Json_get_array ( agent->api_config, "DI" );
          for ( guint cpt = 0; cpt < json_array_get_length ( array ); cpt++ )
           { JsonNode *element = json_array_get_element ( array, cpt );
             gint num = Json_get_int ( element, "num" );
             if ( 0 <= num && num < vars->nbr_entree_tor )
              { vars->DI[num] = element;
                Json_add_bool ( vars->DI[num], "etat", FALSE );
                Json_add_bool ( vars->DI[num], "need_sync", TRUE );
                Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_NOTICE, "New DI '%s' (%s), flip=%d",
                          Json_get_string ( vars->DI[num], "thread_acronyme" ),
                          Json_get_string ( vars->DI[num], "libelle" ),
                          Json_get_bool   ( vars->DI[num], "flip" ));
              } else Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_WARNING, "Map DI: num %d out of range '%d'",
                                num, vars->nbr_entree_tor );
           }
        }
       else Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_ALERT, "Memory Error for DI" );
     }
/***************************************************** Mapping des AnalogOutput ***********************************************/
    Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_INFO, "Allocate %d AO", vars->nbr_sortie_ana );
    if(vars->nbr_sortie_ana)
     { vars->AO = g_try_malloc0( sizeof(JsonNode *) * vars->nbr_sortie_ana );
       if (vars->AO)
        { JsonArray *array = Json_get_array ( agent->api_config, "AO" );
          for ( guint cpt = 0; cpt < json_array_get_length ( array ); cpt++ )
           { JsonNode *element = json_array_get_element ( array, cpt );
             gint num = Json_get_int ( element, "num" );
             if ( 0 <= num && num < vars->nbr_sortie_ana )
              { vars->AO[num] = element;
                Json_add_double ( vars->AO[num], "valeur", 0.0 );
                Json_add_int    ( vars->AO[num], "val_int", 0 );
                Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_NOTICE, "New AO '%s' (%s, %s)",
                          Json_get_string ( vars->AO[num], "thread_acronyme" ),
                          Json_get_string ( vars->AI[num], "libelle" ),
                          Json_get_string ( vars->AI[num], "unite" ) );
              } else Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_WARNING, "map AO: num %d out of range '%d'",
                               num, vars->nbr_sortie_ana );
           }
        }
       else Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_ALERT, "Memory Error for AO" );
     }
/***************************************************** Mapping des DigitalOutput **********************************************/
    Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_INFO, "Allocate %d DO", vars->nbr_sortie_tor );
    if(vars->nbr_sortie_tor)
     { vars->DO = g_try_malloc0( sizeof(JsonNode *) * vars->nbr_sortie_tor );
       if (vars->DO)
        { JsonArray *array = Json_get_array ( agent->api_config, "DO" );
          for ( guint cpt = 0; cpt < json_array_get_length ( array ); cpt++ )
           { JsonNode *element = json_array_get_element ( array, cpt );
             gint num = Json_get_int ( element, "num" );
             if ( 0 <= num && num < vars->nbr_sortie_tor )
              { vars->DO[num] = element;
                Json_add_bool   ( vars->DO[num], "etat", FALSE );
                Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_NOTICE, "New DO '%s' (%s)",
                          Json_get_string ( vars->DO[num], "thread_acronyme" ),
                          Json_get_string ( vars->DO[num], "libelle" ));
              } else Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_WARNING, "map DO: num %d out of range '%d'",
                               num, vars->nbr_sortie_tor );
           }
        }
       else Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_ALERT, " Memory Error for DO" );
     }
/******************************* Recherche des event text EA a raccrocher aux bits internes ***********************************/
    Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_NOTICE, "Module '%s' : io config done",
              Json_get_string ( agent->api_config, "description" ) );
  }
/******************************************************************************************************************************/
/* Recuperer_borne: Recupere les informations d'une borne MODBUS                                                              */
/* Entrée: identifiants des modules et borne                                                                                  */
/* Sortie: ?                                                                                                                  */
/******************************************************************************************************************************/
 static void Modbus_Processer_trame( struct ABLS_AGENT *agent )
  { struct MODBUS_VARS *vars = agent->vars;
    vars->nbr_oct_lu = 0;
    vars->request = FALSE;                                                                       /* Une requete a été traitée */

    if ( (guint16) vars->response.proto_id )
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_WARNING, "Wrong proto_id" );
       Deconnecter_module( agent );
     }

    gint cpt_byte, cpt_poid, cpt;
    vars->date_last_reponse = Modbus_top_ds();                                                        /* Estampillage de la date */
    Agent_send_comm_to_master ( agent, TRUE );
    if (ntohs(vars->response.transaction_id) != vars->transaction_id)                                     /* Mauvaise reponse */
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_ERR, "Wrong transaction_id: attendu %d, recu %d",
                 vars->transaction_id, ntohs(vars->response.transaction_id) );
     }
    if ( vars->response.fct >=0x80 )
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_ERR, "Erreur Reponse, Error %d, Exception code %d",
                 vars->response.fct, (int)vars->response.data[0] );
       Deconnecter_module( agent );
       return;
     }
    switch (vars->mode)
     { case MODBUS_GET_DI:
            for ( cpt_poid = 1, cpt_byte = 1, cpt = 0; cpt<vars->nbr_entree_tor; cpt++)
             { if (vars->DI[cpt])                                                                   /* Si l'entrée est mappée */
                { gint new_etat_int = (vars->response.data[ cpt_byte ] & cpt_poid);
                  gboolean new_etat = (new_etat_int ? TRUE : FALSE);
                  if ( Json_get_bool ( vars->DI[cpt], "flip" ) ) new_etat = new_etat ^ 1;
                  Mqtt_Send_DI ( agent, vars->DI[cpt], new_etat );
                }
               cpt_poid = cpt_poid << 1;
               if (cpt_poid == 256) { cpt_byte++; cpt_poid = 1; }
             }
            vars->mode = MODBUS_GET_AI;
            break;
       case MODBUS_GET_AI:
            for ( cpt = 0; cpt<vars->nbr_entree_ana; cpt++)
             { if (vars->AI[cpt])                                                                   /* Si l'entrée est mappée */
                { gint type_borne = Json_get_int ( vars->AI[cpt], "type_borne" );
                  gboolean new_in_range;
                  gdouble new_valeur;
                  switch( type_borne )
                   { case WAGO_750455:                                                                       /* Borne 4/20 mA */
                      { gint16 new_valeur_int  = (gint16)vars->response.data[ 2*cpt + 1 ] << 5;
                               new_valeur_int |= (gint16)vars->response.data[ 2*cpt + 2 ] >> 3;
                        gdouble min  = Json_get_double ( vars->AI[cpt], "min" );
                        gdouble max  = Json_get_double ( vars->AI[cpt], "max" );
                        new_valeur   = ((gint)new_valeur_int*(max - min))/4095.0 + min;
                        new_in_range = !(vars->response.data[ 2*cpt + 2 ] & 0x03);
                        break;
                      }
                     case WAGO_750461:                                                                         /* Borne PT100 */
                      { gint16 new_valeur_int  = (gint16)vars->response.data[ 2*cpt + 1 ] << 8;
                               new_valeur_int |= (gint16)vars->response.data[ 2*cpt + 2 ];
                        new_valeur  = ((gint)new_valeur_int)/10.0;
                        if (new_valeur_int > -2000 && new_valeur_int < 8500) new_in_range = TRUE; else new_in_range = FALSE;
                        break;
                      }
                     default : new_valeur=0.0; new_in_range=FALSE;
                   }
                  Mqtt_Send_AI ( agent, vars->AI[cpt], new_valeur, new_in_range );
                }
             }
            vars->mode = MODBUS_SET_DO;
            break;
       case MODBUS_SET_DO:
            vars->mode = MODBUS_SET_AO;
            break;
       case MODBUS_SET_AO:
            vars->mode = MODBUS_GET_DI;
            break;
       case MODBUS_GET_DESCRIPTION:
          { gchar chaine[32];
            guint taille;
            memset ( chaine, 0, sizeof(chaine) );
            taille = vars->response.data[0];
            if (taille>=sizeof(chaine)) taille=sizeof(chaine)-1;
            chaine[0] = ntohs( (gint16)vars->response.data[1] );
            chaine[2] = ntohs( (gint16)vars->response.data[3] );
            chaine[taille] = 0;
            Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_INFO, "Description (size %d) = '%s'", taille, chaine );
            vars->mode = MODBUS_GET_FIRMWARE;
            break;
         }
       case MODBUS_GET_FIRMWARE:
          { gchar chaine[64];
            guint taille;
            memset ( chaine, 0, sizeof(chaine) );
            taille = vars->response.data[0];
            if (taille>=sizeof(chaine)) taille=sizeof(chaine)-1;
            chaine[0] = ntohs( (gint16)vars->response.data[1] );
            chaine[2] = ntohs( (gint16)vars->response.data[3] );
            chaine[taille] = 0;
            Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_INFO, "Firmware (size %d) = '%s'", taille, chaine );
            vars->mode = MODBUS_INIT_WATCHDOG1;
            break;
         }
       case MODBUS_INIT_WATCHDOG1:
            Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_DEBUG, "Watchdog1 = %d %d",
                      ntohs( *(gint16 *)((gchar *)&vars->response.data + 0) ),
                      ntohs( *(gint16 *)((gchar *)&vars->response.data + 2) )
                    );
            vars->mode = MODBUS_INIT_WATCHDOG2;
            break;
       case MODBUS_INIT_WATCHDOG2:
            Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_DEBUG, "Watchdog2 = %d %d",
                      ntohs( *(gint16 *)((gchar *)&vars->response.data + 0) ),
                      ntohs( *(gint16 *)((gchar *)&vars->response.data + 2) )
                    );
            vars->mode = MODBUS_INIT_WATCHDOG3;
            break;
       case MODBUS_INIT_WATCHDOG3:
            Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_DEBUG, "Watchdog3 = %d %d",
                      ntohs( *(gint16 *)((gchar *)&vars->response.data + 0) ),
                      ntohs( *(gint16 *)((gchar *)&vars->response.data + 2) )
                    );
            vars->mode = MODBUS_INIT_WATCHDOG4;
            break;
       case MODBUS_INIT_WATCHDOG4:
            Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_DEBUG, "Watchdog4 = %d %d",
                     ntohs( *(gint16 *)((gchar *)&vars->response.data + 0) ),
                      ntohs( *(gint16 *)((gchar *)&vars->response.data + 2) )
                    );
            vars->mode = MODBUS_GET_NBR_AI;
            break;
       case MODBUS_GET_NBR_AI:
             { vars->nbr_entree_ana = ntohs( *(gint16 *)((gchar *)&vars->response.data + 1) ) / 16;
               Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_INFO, "Get %03d Entree ANA", vars->nbr_entree_ana );
               vars->mode = MODBUS_GET_NBR_AO;
             }
            break;
       case MODBUS_GET_NBR_AO:
             { vars->nbr_sortie_ana = ntohs( *(gint16 *)((gchar *)&vars->response.data + 1) ) / 16;
               Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_INFO, "Get %03d Sortie ANA", vars->nbr_sortie_ana );
               vars->mode = MODBUS_GET_NBR_DI;
             }
            break;
       case MODBUS_GET_NBR_DI:
             { gint nbr;
               nbr = ntohs( *(gint16 *)((gchar *)&vars->response.data + 1) );
               vars->nbr_entree_tor = nbr;
               Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_INFO, "Get %03d Entree TOR", vars->nbr_entree_tor );
               vars->mode = MODBUS_GET_NBR_DO;
             }
            break;
       case MODBUS_GET_NBR_DO:
             { vars->nbr_sortie_tor = ntohs( *(gint16 *)((gchar *)&vars->response.data + 1) );
               Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_INFO, "Get %03d Sortie TOR", vars->nbr_sortie_tor );
               Modbus_load_io_config( agent );                                                  /* Initialise les IO modules */
               JsonNode *RootNode = Json_create ();                                          /* Envoi de la conf a l'API */
               if (!RootNode) break;
               Json_add_string ( RootNode, "agent_tech_id", agent->agent_tech_id );
               Json_add_int    ( RootNode, "nbr_entree_tor", vars->nbr_entree_tor );
               Json_add_int    ( RootNode, "nbr_entree_ana", vars->nbr_entree_ana );
               Json_add_int    ( RootNode, "nbr_sortie_tor", vars->nbr_sortie_tor );
               Json_add_int    ( RootNode, "nbr_sortie_ana", vars->nbr_sortie_ana );
               JsonNode *API_result = Http_Post_to_global_API ( agent, "/run/modbus/add/io", RootNode );
               Json_unref ( API_result );
               Json_unref ( RootNode );
               vars->mode = MODBUS_GET_DI;
             }
            break;
     }
  }
/******************************************************************************************************************************/
/* Recuperer_borne: Recupere les informations d'une borne MODBUS                                                              */
/* Entrée: identifiants des modules et borne                                                                                  */
/* Sortie: ?                                                                                                                  */
/******************************************************************************************************************************/
 static void Recuperer_reponse_module( struct ABLS_AGENT *agent )
  { struct MODBUS_VARS *vars = agent->vars;
    fd_set fdselect;
    struct timeval tv;
    gint retval, cpt;

    if (vars->date_last_reponse + 60 < time(NULL))                                           /* Detection attente trop longue */
     { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_WARNING,
                "Timeout agent started=%d, mode=%02d, "
                "transactionID=%06d, nbr_deconnect=%02d, last_reponse=%03ds ago, retente=in %03ds, date_next_eana=in %03ds",
                 vars->started, vars->mode, vars->transaction_id, vars->nbr_deconnect,
                (time(NULL) - vars->date_last_reponse),
                (vars->date_retente > time(NULL)   ? (vars->date_retente   - time(NULL)) : -1),
                (vars->date_next_eana > time(NULL) ? (vars->date_next_eana - time(NULL)) : -1)
               );
       Deconnecter_module( agent );
       return;
     }

    FD_ZERO(&fdselect);
    FD_SET(vars->connexion, &fdselect );
    tv.tv_sec = 0;
    tv.tv_usec= 1000;                                                                               /* Attente d'un caractere */
    retval = select(vars->connexion+1, &fdselect, NULL, NULL, &tv );

    if ( retval>0 && FD_ISSET(vars->connexion, &fdselect) )
     { guint bute;
       if (vars->nbr_oct_lu<TAILLE_ENTETE_MODBUS)
            { bute = TAILLE_ENTETE_MODBUS; }
       else { bute = TAILLE_ENTETE_MODBUS + ntohs(vars->response.taille); }

       if (bute>=sizeof(struct TRAME_MODBUS_REPONSE))
        { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_ERR,
                   "bute = %d >= %d (sizeof(agent->reponse)=%d, taille recue = %d)",
                    bute, sizeof(struct TRAME_MODBUS_REPONSE), sizeof(vars->response), ntohs(vars->response.taille) );
          Deconnecter_module( agent );
          return;
        }

       cpt = read( vars->connexion, (unsigned char *)&vars->response + vars->nbr_oct_lu, bute-vars->nbr_oct_lu );
       if (cpt>=0)
        { vars->nbr_oct_lu += cpt;
          if (vars->nbr_oct_lu >= TAILLE_ENTETE_MODBUS + ntohs(vars->response.taille))
           { Modbus_Processer_trame( agent ); }                                    /* Si l'on a trouvé une trame complète !! */
        }
       else
        { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_WARNING, "Read Error. Get %d, error %s", cpt, strerror(errno) );
          Deconnecter_module ( agent );
        }
      }
  }
/******************************************************************************************************************************/
/* main: Point d'entrée de l'agent modbus                                                                                     */
/* Entrée: argc/argv                                                                                                          */
/* Sortie: code de retour process                                                                                             */
/******************************************************************************************************************************/
 gint main ( gint argc, gchar *argv[] )
  { struct ABLS_AGENT *agent = Agent_init ( argv[0], "modbus", ABLS_AGENT_MODBUS_VERSION, sizeof(struct MODBUS_VARS), argc, argv );
    struct MODBUS_VARS *vars = agent->vars;

    Mqtt_subscribe ( agent->mqtt_local, "SYNC_INPUT/%s", agent->agent_tech_id );
    Agent_is_ready ( agent );

    while(agent->Agent_run == AGENT_IS_RUNNING)                                             /* On tourne tant que necessaire */
     { Agent_loop ( agent );                                            /* Loop sur l'agent pour mettre a jour la telemetrie */
/****************************************************** Ecoute du master ******************************************************/
       JsonNode *mqtt_local_message;
       while ( (mqtt_local_message = Agent_get_mqtt_local_message(agent)) != NULL )
        { if ( Mqtt_topic_is(mqtt_local_message, 2, "SET_DO", agent->agent_tech_id) )
           { Modbus_SET_DO ( agent, mqtt_local_message ); }
          else if ( Mqtt_topic_is(mqtt_local_message, 2, "SET_AO", agent->agent_tech_id) )
           { Modbus_SET_AO ( agent, mqtt_local_message ); }
          else if ( Mqtt_topic_is(mqtt_local_message, 2, "SYNC_INPUT", agent->agent_tech_id) )
           { Modbus_Sync_INPUT_to_master ( agent ); }
          Json_unref ( mqtt_local_message );
        }

/****************************************************** Ecoute de l'api *******************************************************/
       JsonNode *mqtt_api_message;
       while ( (mqtt_api_message = Agent_get_mqtt_api_message(agent)) != NULL )
        { Json_unref ( mqtt_api_message ); }

/********************************************* Début de l'interrogation du module *********************************************/
       if ( vars->started == FALSE )                                               /* Si attente retente, on change de module */
        { if ( vars->date_retente <= time(NULL) && Connecter_module(agent)==FALSE )
           { Info( __func__, agent->agent_classe, agent->agent_tech_id, LOG_INFO, "Module DOWN. retrying in %ds", MODBUS_RETRY/10 );
             vars->date_retente = time(NULL) + MODBUS_RETRY;
           }
        }
       else for (gint i=0; i<4; i++)                                     /* 1 tour programme = 4 itérations GET (DI/DO/AI/AO) */
        { if ( vars->request )                                                           /* Requete en cours pour ce module ? */
           { Recuperer_reponse_module ( agent ); }
          else
           { if (vars->date_next_eana<time(NULL))                                    /* Gestion décalée des I/O Analogiques */
              { vars->date_next_eana = time(NULL) + MBUS_TEMPS_UPDATE_IO_ANA;                        /* Tous les 5 dixiemes */
                vars->do_check_eana = TRUE;
              }
             switch (vars->mode)
              { case MODBUS_GET_DESCRIPTION: Interroger_description( agent ); break;
                case MODBUS_GET_FIRMWARE   : Interroger_firmware( agent ); break;
                case MODBUS_INIT_WATCHDOG1 : Init_watchdog1( agent ); break;
                case MODBUS_INIT_WATCHDOG2 : Init_watchdog2( agent ); break;
                case MODBUS_INIT_WATCHDOG3 : Init_watchdog3( agent ); break;
                case MODBUS_INIT_WATCHDOG4 : Init_watchdog4( agent ); break;
                case MODBUS_GET_NBR_AI     : Interroger_nbr_entree_ANA( agent ); break;
                case MODBUS_GET_NBR_AO     : Interroger_nbr_sortie_ANA( agent ); break;
                case MODBUS_GET_NBR_DI     : Interroger_nbr_entree_TOR( agent ); break;
                case MODBUS_GET_NBR_DO     : Interroger_nbr_sortie_TOR( agent ); break;
                case MODBUS_GET_DI         : if (vars->nbr_entree_tor) Interroger_entree_tor( agent );
                                             else vars->mode = MODBUS_GET_AI;
                                             break;
                case MODBUS_GET_AI         : if (vars->nbr_entree_ana && vars->do_check_eana)
                                              { Interroger_entree_ana( agent ); }
                                             else vars->mode = MODBUS_SET_DO;
                                             break;
                case MODBUS_SET_DO         : if (vars->nbr_sortie_tor) Interroger_sortie_tor( agent );
                                             else vars->mode = MODBUS_SET_AO;
                                             break;
                case MODBUS_SET_AO         : if (vars->nbr_sortie_ana && vars->do_check_eana)
                                              { Interroger_sortie_ana( agent ); }
                                             else vars->mode = MODBUS_GET_DI;
                                             vars->do_check_eana = FALSE;                                /* Le check est fait */
                                             break;
              }
           }
        }
     }

    Deconnecter_module(agent);
    Agent_end(agent);
    return 0;
  }
/*----------------------------------------------------------------------------------------------------------------------------*/
