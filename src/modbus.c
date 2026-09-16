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

 struct ABLS_AGENT *Agent = NULL;
 struct MODBUS_VARS *Agent_vars = NULL;

/******************************************************************************************************************************/
/* Modbus_SET_DO: Met a jour une sortie TOR en fonction du jsonnode en parametre                                              */
/* Entrée: buffer Json reçu                                                                                                   */
/* Sortie: Niet                                                                                                               */
/******************************************************************************************************************************/
 static void Modbus_SET_DO ( JsonNode *msg )
  { gchar *msg_agent_tech_id  = Json_get_string ( msg, "mqtt_topic_lvl1" );
    gchar *msg_agent_acronyme = Json_get_string ( msg, "mqtt_topic_lvl2" );
    gchar *msg_tech_id        = Json_get_string ( msg, "tech_id" );
    gchar *msg_acronyme       = Json_get_string ( msg, "acronyme" );

    if (!msg_agent_tech_id)
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_ERR, "requete mal formée manque msg_agent_tech_id" ); }
    else if (!msg_agent_acronyme)
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_ERR, "requete mal formée manque msg_agent_acronyme" ); }
    else if (strcasecmp (msg_agent_tech_id, Agent->agent_tech_id))
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_DEBUG, "Pas pour nous" ); }
    else if (!Json_has_member ( msg, "etat" ))
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_ERR, "Requete mal formée manque etat" ); }
    else
     { gboolean etat = Json_get_bool ( msg, "etat" );
       for (gint num=0; num<Agent_vars->nbr_sortie_tor; num++)
        { if ( Agent_vars->DO && Agent_vars->DO[num] &&
               !strcasecmp ( Json_get_string(Agent_vars->DO[num], "agent_acronyme"), msg_agent_acronyme ) )
           { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_INFO, "SET_DO '%s:%s'/'%s:%s'=%d",
                       msg_agent_tech_id, msg_agent_acronyme, msg_tech_id, msg_acronyme, etat );
             Json_add_bool ( Agent_vars->DO[num], "etat", etat );
             break;
           }
        }
     }
  }
/******************************************************************************************************************************/
/* Modbus_SET_AO: Met a jour une sortie ANA en fonction du jsonnode en parametre                                              */
/* Entrée: le buffer Json reçu                                                                                                */
/* Sortie: Niet                                                                                                               */
/******************************************************************************************************************************/
 static void Modbus_SET_AO ( JsonNode *msg )
  { gchar *msg_agent_tech_id  = Json_get_string ( msg, "mqtt_topic_lvl1" );
    gchar *msg_agent_acronyme = Json_get_string ( msg, "mqtt_topic_lvl2" );
    gchar *msg_tech_id        = Json_get_string ( msg, "tech_id" );
    gchar *msg_acronyme       = Json_get_string ( msg, "acronyme" );

    if (!msg_agent_tech_id)
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_ERR, "requete mal formée manque msg_agent_tech_id" ); }
    else if (!msg_agent_acronyme)
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_ERR, "requete mal formée manque msg_agent_acronyme" ); }
    else if (strcasecmp (msg_agent_tech_id, Agent->agent_tech_id))
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_DEBUG, "Pas pour nous" ); }
    else if (!Json_has_member ( msg, "valeur" ))
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_ERR, "Requete mal formée manque etat" ); }
    else
     { gdouble valeur = Json_get_double ( msg, "valeur" );
       for (gint num=0; num<Agent_vars->nbr_sortie_ana; num++)
        { if ( Agent_vars->AO && Agent_vars->AO[num] &&
               !strcasecmp ( Json_get_string(Agent_vars->AO[num], "thread_acronyme"), msg_agent_acronyme ) )
           { gint type_borne = Json_get_int    ( Agent_vars->AO[num], "type_borne" );
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
             Json_add_int ( Agent_vars->AO[num], "val_int", new_val_int );
             Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_NOTICE, "SET_AO '%s:%s'/'%s:%s'=%f (val_int=%d)",
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
 static void Modbus_Sync_INPUT_to_master ( void )
  { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_INFO, "Syncing IO to master" );

    for ( gint cpt = 0; cpt<Agent_vars->nbr_entree_tor; cpt++)
     { if (Agent_vars->DI[cpt]) Json_add_bool ( Agent_vars->DI[cpt], "need_sync", TRUE ); }

    for ( gint cpt = 0; cpt<Agent_vars->nbr_entree_ana; cpt++)
     { if (Agent_vars->AI[cpt]) Json_add_bool ( Agent_vars->AI[cpt], "need_sync", TRUE ); }
  }
/******************************************************************************************************************************/
/* Deconnecter: Deconnexion du agent                                                                                         */
/* Entrée: un id                                                                                                              */
/* Sortie: néant                                                                                                              */
/******************************************************************************************************************************/
 static void Deconnecter_module ( void )
  { if (Agent_vars->started == FALSE) return;
    close ( Agent_vars->connexion );
    Agent_vars->connexion = 0;
    Agent_vars->started = FALSE;
    Agent_vars->request = FALSE;
    Agent_vars->nbr_deconnect++;
    Agent_vars->top_next_reconnect = Top_set_next_in(MODBUS_TOP_NEXT_RETRY);
    if (Agent_vars->DI) { g_free(Agent_vars->DI); Agent_vars->DI = NULL; }
    if (Agent_vars->DO) { g_free(Agent_vars->DO); Agent_vars->DO = NULL; }
    if (Agent_vars->AI) { g_free(Agent_vars->AI); Agent_vars->AI = NULL; }
    if (Agent_vars->AO) { g_free(Agent_vars->AO); Agent_vars->AO = NULL; }
    Agent_vars->nbr_entree_tor = 0;
    Agent_vars->nbr_entree_ana = 0;
    Agent_vars->nbr_sortie_ana = 0;
    Agent_vars->nbr_sortie_tor = 0;
    Agent_send_comm_to_master ( Agent, FALSE );
    Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_INFO, "Module '%s' disconnected", Agent_vars->hostname );
  }
/******************************************************************************************************************************/
/* Connecter: Tentative de connexion au serveur                                                                               */
/* Entrée: une nom et un password                                                                                             */
/* Sortie: les variables globales sont initialisées, FALSE si pb                                                              */
/******************************************************************************************************************************/
 static gboolean Connecter_module ( void )
  { struct addrinfo *result, *rp;
    struct timeval sndtimeout;
    struct addrinfo hints;
    gint connexion = 0, s;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */

    sndtimeout.tv_sec  = 10;
    sndtimeout.tv_usec =  0;

    Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_DEBUG, "Trying to connect agent to '%s'", Agent_vars->hostname );

    s = getaddrinfo( Agent_vars->hostname, "502", &hints, &result);
    if (s != 0)
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_ERR,
             "getaddrinfo Failed for agent %s (%s)", Agent_vars->hostname, gai_strerror(s) );
       return(FALSE);
     }

   /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully connect(2).
       If socket(2) (or connect(2)) fails, we (close the socket
       and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next)
     { connexion = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
       if (connexion == -1)
        { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_ERR,
                "Socket creation failed for modbus '%s'", Agent_vars->hostname );
          continue;
        }

       if ( setsockopt ( connexion, SOL_SOCKET, SO_SNDTIMEO, (char *)&sndtimeout, sizeof(sndtimeout)) < 0 )
        { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_ERR,
                "Socket Set Options failed for modbus '%s'", Agent_vars->hostname );
          continue;
        }

       if (connect(connexion, rp->ai_addr, rp->ai_addrlen) != -1)
        { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_INFO,
                "Using family=%d for host '%s'", rp->ai_family, Agent_vars->hostname );
          break;  /* Success */
        }
       else
        { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_ERR,
                   "'connexion refused by agent '%s' family=%d error '%s'",
                   Agent_vars->hostname, rp->ai_family, strerror(errno) );
        }
       close(connexion);                                                       /* Suppression de la socket qui n'a pu aboutir */
     }
    freeaddrinfo(result);
    if (rp == NULL) return(FALSE);                                                                     /* Erreur de connexion */

    fcntl( connexion, F_SETFL, SO_KEEPALIVE | SO_REUSEADDR );
    Agent_vars->connexion         = connexion;                                                            /* Sauvegarde du fd */
    Agent_vars->top_last_response = Top_set_now();
    Agent_vars->top_next_reconnect= 0;
    Agent_vars->transaction_id    = 1;
    Agent_vars->started           = TRUE;
    Agent_vars->mode              = MODBUS_GET_DESCRIPTION;
    Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_NOTICE, "Module '%s' Connected", Agent_vars->hostname );

    return(TRUE);
  }
/******************************************************************************************************************************/
/* Interroger_description : envoie une commande d'identification au agent                                                    */
/* Entrée: L'id de la transmission, et la trame a transmettre                                                                 */
/******************************************************************************************************************************/
 static void Interroger_description( void )
  { struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */

    Agent_vars->transaction_id++;
    requete.transaction_id = htons(Agent_vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    requete.taille         = htons( 0x006 );                                                /* taille, en comptant le unit_id */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_READ_REGISTER;
    requete.adresse        = htons( 0x2020 );
    requete.nbr            = htons( 16 );

    gint retour = write ( Agent_vars->connexion, &requete, 12 );
    if ( retour != 12 )                                                                                /* Envoi de la requete */
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_WARNING,
               "Failed for agent '%s': error %d/%s", Agent_vars->hostname, retour, strerror(errno) );
       Deconnecter_module();
     }
    else
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_DEBUG, "OK" );
       Agent_vars->request = TRUE;                                                                      /* Une requete a élé lancée */
     }
  }
/******************************************************************************************************************************/
/* Interroger_description : envoie une commande d'identification au agent                                                    */
/* Entrée: L'id de la transmission, et la trame a transmettre                                                                 */
/******************************************************************************************************************************/
 static void Interroger_firmware( void )
  { struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */

    Agent_vars->transaction_id++;
    requete.transaction_id = htons(Agent_vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    requete.taille         = htons( 0x006 );                                                /* taille, en comptant le unit_id */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_READ_REGISTER;
    requete.adresse        = htons( 0x2023 );
    requete.nbr            = htons( 16 );

    gint retour = write ( Agent_vars->connexion, &requete, 12 );
    if ( retour != 12 )                                                                                /* Envoi de la requete */
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_WARNING,
               "Failed for agent '%s': error %d/%s", Agent_vars->hostname, retour, strerror(errno) );
       Deconnecter_module();
     }
    else
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_DEBUG, "OK" );
       Agent_vars->request = TRUE;                                                                    /* Une requete a élé lancée */
     }
  }
/******************************************************************************************************************************/
/* Interroger_borne: Interrogation d'une borne du agent                                                                      */
/* Entrée: identifiants des modules et borne                                                                                  */
/* Sortie: néant                                                                                                              */
/******************************************************************************************************************************/
 static void Init_watchdog1( void )
  { struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */

    Agent_vars->transaction_id++;
    requete.transaction_id = htons(Agent_vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    requete.taille         = htons( 0x0006 );                                               /* taille, en comptant le unit_id */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_WRITE_REGISTER;
    requete.adresse        = htons( 0x100A );                                                                   /* Stop Timer */
    requete.valeur         = htons( 0x0000 );

    gint retour = write ( Agent_vars->connexion, &requete, 12 );
    if ( retour != 12 )                                                                                /* Envoi de la requete */
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_WARNING,
               "Failed for agent '%s': error %d/%s", Agent_vars->hostname, retour, strerror(errno) );
       Deconnecter_module();
     }
    else
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_DEBUG, "OK" );
       Agent_vars->request = TRUE;                                                                    /* Une requete a élé lancée */
     }
  }
/******************************************************************************************************************************/
/* Interroger_borne: Interrogation d'une borne du agent                                                                      */
/* Entrée: identifiants des modules et borne                                                                                  */
/* Sortie: ?                                                                                                                  */
/******************************************************************************************************************************/
 static void Init_watchdog2( void )
  { struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */

    Agent_vars->transaction_id++;
    requete.transaction_id = htons(Agent_vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    requete.taille         = htons( 0x0006 );                                               /* taille, en comptant le unit_id */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_WRITE_REGISTER;
    requete.adresse        = htons( 0x1009 );                                   /* Close MODBUS socket after watchdog timeout */
    requete.valeur         = htons( 0x0001 );

    gint retour = write ( Agent_vars->connexion, &requete, 12 );
    if ( retour != 12 )                                                                                /* Envoi de la requete */
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_WARNING,
             "Failed for agent '%s': error %d/%s", Agent_vars->hostname, retour, strerror(errno) );
       Deconnecter_module();
     }
    else
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_DEBUG, "OK" );
       Agent_vars->request = TRUE;                                                /* Une requete a élé lancée */
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
 static void Init_watchdog3( void )
  { struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */

    Agent_vars->transaction_id++;
    requete.transaction_id = htons(Agent_vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    requete.taille         = htons( 0x0006 );                                               /* taille, en comptant le unit_id */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_WRITE_REGISTER;
    requete.adresse        = htons( 0x1000 );                                                       /* Watchdog Time register */
    requete.valeur         = htons( Agent_vars->watchdog );                        /* coupure sortie, en 100ième de secondes  */

    gint retour = write ( Agent_vars->connexion, &requete, 12 );
    if ( retour != 12 )                                                                                /* Envoi de la requete */
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_WARNING,
             "Failed for agent '%s': error %d/%s", Agent_vars->hostname, retour, strerror(errno) );
       Deconnecter_module();
     }
    else
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_DEBUG, "OK" );
       Agent_vars->request = TRUE;                                                /* Une requete a élé lancée */
     }
  }
/******************************************************************************************************************************/
/* Interroger_borne: Interrogation d'une borne du agent                                                                      */
/* Entrée: identifiants des modules et borne                                                                                  */
/* Sortie: ?                                                                                                                  */
/******************************************************************************************************************************/
 static void Init_watchdog4( void )
  { struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */

    Agent_vars->transaction_id++;
    requete.transaction_id = htons(Agent_vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    requete.taille         = htons( 0x0006 );                                               /* taille, en comptant le unit_id */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_WRITE_REGISTER;
    requete.adresse        = htons( 0x100A );
    requete.valeur         = htons( 0x0001 );                                                                  /* Start Timer */

    gint retour = write ( Agent_vars->connexion, &requete, 12 );
    if ( retour != 12 )                                                                                /* Envoi de la requete */
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_WARNING,
                "Failed for agent '%s': error %d/%s", Agent_vars->hostname, retour, strerror(errno) );
       Deconnecter_module();
     }
    else
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_DEBUG, "OK" );
       Agent_vars->request = TRUE;                                                /* Une requete a élé lancée */
     }
  }
/******************************************************************************************************************************/
/* Interroger_nbr_entree_ANA : Demander au agent d'envoyer son nombre d'entree ANALOGIQUE                                    */
/* Entrée: L'id de la transmission, et la trame a transmettre                                                                 */
/******************************************************************************************************************************/
 static void Interroger_nbr_entree_ANA( void )
  { struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */

    Agent_vars->transaction_id++;
    requete.transaction_id = htons(Agent_vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    requete.taille         = htons( 0x006 );                                                /* taille, en comptant le unit_id */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_READ_REGISTER;
    requete.adresse        = htons( 0x1023 );
    requete.nbr            = htons( 0x0001 );

    gint retour = write ( Agent_vars->connexion, &requete, 12 );
    if ( retour != 12 )                                                                                /* Envoi de la requete */
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_WARNING,
               "Failed for agent '%s': error %d/%s", Agent_vars->hostname, retour, strerror(errno) );
       Deconnecter_module();
     }
    else
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_DEBUG, "OK" );
       Agent_vars->request = TRUE;                                                                    /* Une requete a élé lancée */
     }
  }
/******************************************************************************************************************************/
/* Interroger_nbr_entree_ANA : Demander au agent d'envoyer son nombre de sortie ANALOGIQUE                                   */
/* Entrée: L'id de la transmission, et la trame a transmettre                                                                 */
/******************************************************************************************************************************/
 static void Interroger_nbr_sortie_ANA( void )
  { struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */

    Agent_vars->transaction_id++;
    requete.transaction_id = htons(Agent_vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    requete.taille         = htons( 0x006 );                                                /* taille, en comptant le unit_id */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_READ_REGISTER;
    requete.adresse        = htons( 0x1022 );
    requete.nbr            = htons( 0x0001 );

    gint retour = write ( Agent_vars->connexion, &requete, 12 );
    if ( retour != 12 )                                                                                /* Envoi de la requete */
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_WARNING,
               "Failed for agent '%s': error %d/%s", Agent_vars->hostname, retour, strerror(errno) );
       Deconnecter_module();
     }
    else
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_DEBUG, "OK" );
       Agent_vars->request = TRUE;                                                                    /* Une requete a élé lancée */
     }
  }
/******************************************************************************************************************************/
/* Interroger_nbr_entree_TOR : Demander au agent d'envoyer son nombre d'entree TOR                                           */
/* Entrée: L'id de la transmission, et la trame a transmettre                                                                 */
/******************************************************************************************************************************/
 static void Interroger_nbr_entree_TOR( void )
  { struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */

    Agent_vars->transaction_id++;
    requete.transaction_id = htons(Agent_vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    requete.taille         = htons( 0x006 );                                                /* taille, en comptant le unit_id */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_READ_REGISTER;
    requete.adresse        = htons( 0x1025 );
    requete.nbr            = htons( 0x0001 );

    gint retour = write ( Agent_vars->connexion, &requete, 12 );
    if ( retour != 12 )                                                                                /* Envoi de la requete */
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_WARNING,
               "Failed for agent '%s': error %d/%s", Agent_vars->hostname, retour, strerror(errno) );
       Deconnecter_module();
     }
    else
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_DEBUG, "OK" );
       Agent_vars->request = TRUE;                                                                    /* Une requete a élé lancée */
     }
  }
/******************************************************************************************************************************/
/* Interroger_nbr_sortie_TOR : Demander au agent d'envoyer son nombre de sortie TOR                                          */
/* Entrée: L'id de la transmission, et la trame a transmettre                                                                 */
/******************************************************************************************************************************/
 static void Interroger_nbr_sortie_TOR( void )
  { struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */

    Agent_vars->transaction_id++;
    requete.transaction_id = htons(Agent_vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    requete.taille         = htons( 0x006 );                                                /* taille, en comptant le unit_id */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_READ_REGISTER;
    requete.adresse        = htons( 0x1024 );
    requete.nbr            = htons( 0x0001 );

    gint retour = write ( Agent_vars->connexion, &requete, 12 );
    if ( retour != 12 )                                                                                /* Envoi de la requete */
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_WARNING,
             "Failed for agent '%s': error %d/%s", Agent_vars->hostname, retour, strerror(errno) );
       Deconnecter_module();
     }
    else
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_DEBUG, "OK" );
       Agent_vars->request = TRUE;                                                                      /* Une requete a élé lancée */
     }
  }
/******************************************************************************************************************************/
/* Interroger_borne: Interrogation d'une borne du agent                                                                      */
/* Entrée: identifiants des modules et borne                                                                                  */
/* Sortie: ?                                                                                                                  */
/******************************************************************************************************************************/
 static void Interroger_entree_tor( void )
  { struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */

    Agent_vars->transaction_id++;
    requete.transaction_id = htons(Agent_vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    requete.taille         = htons( 0x0006 );                                               /* taille, en comptant le unit_id */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_READ_COIL;
    requete.adresse        = 0x00;
    requete.nbr            = htons( Agent_vars->nbr_entree_tor );

    gint retour = write ( Agent_vars->connexion, &requete, 12 );
    if ( retour != 12 )                                                                                /* Envoi de la requete */
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_WARNING,
             "Failed for agent '%s': error %d/%s", Agent_vars->hostname, retour, strerror(errno) );
       Deconnecter_module();
     }
    else { Agent_vars->request = TRUE; }                                                                /* Une requete a élé lancée */
  }
/******************************************************************************************************************************/
/* Interroger_entree_ana: Interrogation des entrees analogique d'un agent wago                                               */
/* Entrée: identifiants des modules et borne                                                                                  */
/* Sortie: ?                                                                                                                  */
/******************************************************************************************************************************/
 static void Interroger_entree_ana( void )
  { struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */

    Agent_vars->transaction_id++;
    requete.transaction_id = htons(Agent_vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    requete.taille         = htons( 0x0006 );                                               /* taille, en comptant le unit_id */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_READ_REGISTER;
    requete.adresse        = 0x00;
    requete.nbr            = htons( Agent_vars->nbr_entree_ana );

    gint retour = write ( Agent_vars->connexion, &requete, 12 );
    if ( retour != 12 )                                                                                /* Envoi de la requete */
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_WARNING,
             "Failed for agent '%s': error %d/%s", Agent_vars->hostname, retour, strerror(errno) );
       Deconnecter_module();
     }
    else { Agent_vars->request = TRUE; }                                                                /* Une requete a élé lancée */
  }
/******************************************************************************************************************************/
/* Interroger_borne: Interrogation d'une borne du agent                                                                      */
/* Entrée: identifiants des modules et borne                                                                                  */
/* Sortie: ?                                                                                                                  */
/******************************************************************************************************************************/
 static void Interroger_sortie_tor( void )
  { struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */
    gint taille, nbr_data;

    memset(&requete, 0, sizeof(requete) );                                               /* Mise a zero globale de la requete */
    nbr_data = ((Agent_vars->nbr_sortie_tor-1)/8)+1;
    Agent_vars->transaction_id++;
    requete.transaction_id = htons(Agent_vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    taille                 = 0x0007 + nbr_data;
    requete.taille         = htons( taille );                                                                       /* taille */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_WRITE_MULTIPLE_COIL;
    requete.adresse        = 0x00;
    requete.nbr            = htons( Agent_vars->nbr_sortie_tor );                                                    /* bit count */
    requete.data[2]        = nbr_data;                                                                          /* Byte count */

    if (Agent_vars->DO)
     { gint cpt_poid, cpt_byte, cpt;
       for ( cpt_poid = 1, cpt_byte = 3, cpt = 0; cpt<Agent_vars->nbr_sortie_tor; cpt++ )
        { if (cpt_poid == 256) { cpt_byte++; cpt_poid = 1; }
          if ( Agent_vars->DO[cpt] )
           { if (Json_get_bool ( Agent_vars->DO[cpt], "etat" )) { requete.data[cpt_byte] |= cpt_poid; } }
          cpt_poid = cpt_poid << 1;
        }
     }

    gint retour = write ( Agent_vars->connexion, &requete, taille+6 );
    if ( retour != taille+6 )                                                                          /* Envoi de la requete */
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_WARNING,
             "Failed for agent '%s': error %d/%s", Agent_vars->hostname, retour, strerror(errno) );
       Deconnecter_module();
     }
    else { Agent_vars->request = TRUE; }                                                                /* Une requete a élé lancée */
  }
/******************************************************************************************************************************/
/* Interroger_sortie_ana: Envoie les informations liées aux sorties ANA du agent                                             */
/* Entrée: le agent à interroger                                                                                             */
/* Sortie: néant                                                                                                              */
/******************************************************************************************************************************/
 static void Interroger_sortie_ana( void )
  { struct TRAME_MODBUS_REQUETE requete;                                                     /* Definition d'une trame MODBUS */
    gint taille;

    memset(&requete, 0, sizeof(requete) );                                               /* Mise a zero globale de la requete */
    Agent_vars->transaction_id++;
    requete.transaction_id = htons(Agent_vars->transaction_id);
    requete.proto_id       = 0x00;                                                                            /* -> 0 = MOBUS */
    taille                 = 0x0006 + (Agent_vars->nbr_sortie_ana*2 + 1);
    requete.taille         = htons( taille );                                                                       /* taille */
    requete.unit_id        = 0x00;                                                                                    /* 0xFF */
    requete.fct            = MBUS_WRITE_MULTIPLE_REGISTER;
    requete.adresse        = 0x00;
    requete.nbr            = htons( Agent_vars->nbr_sortie_ana );                                                      /* bit count */
    requete.data[2]        = (Agent_vars->nbr_sortie_ana*2);                                                          /* Byte count */

    if (Agent_vars->AO)
     { gint cpt_byte, cpt;
       for ( cpt_byte = 3, cpt = 0; cpt<Agent_vars->nbr_sortie_ana; cpt++)
        { if (Agent_vars->AO[cpt])
           { gint val_int = Json_get_int ( Agent_vars->AO[cpt], "val_int" );
             requete.data [cpt_byte  ] =  val_int >> 8;                                                /* Octet de poids fort */
             requete.data [cpt_byte+1] =  val_int & 0xFF;                                            /* Octet de poids faible */
             cpt_byte += 2;
           }
        }
     }

    gint retour = write ( Agent_vars->connexion, &requete, taille+6 );
    if ( retour != taille+6 )                                                                          /* Envoi de la requete */
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_WARNING,
             "Failed for agent '%s': error %d/%s", Agent_vars->hostname, retour, strerror(errno) );
       Deconnecter_module();
     }
    else { Agent_vars->request = TRUE; }                                                                /* Une requete a élé lancée */
  }
/******************************************************************************************************************************/
/* Modbus_load_io_config : Charge les données des IO du agent                                                                */
/* Entrée : la structure referencant le agent                                                                                */
/* Sortie : rien                                                                                                              */
/******************************************************************************************************************************/
 static void Modbus_load_io_config ( void )
  {
/***************************************************** Mapping des AnalogInput ************************************************/
    Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_INFO, "Allocate %d AI", Agent_vars->nbr_entree_ana );
    if(Agent_vars->nbr_entree_ana)
     { Agent_vars->AI = g_try_malloc0( sizeof(JsonNode *) * Agent_vars->nbr_entree_ana );
       if (Agent_vars->AI)
        { JsonArray *array = Agent_config_get_array ( Agent, "AI" );
          for ( guint cpt = 0; cpt < json_array_get_length ( array ); cpt++ )
           { JsonNode *element = json_array_get_element ( array, cpt );
             gint num = Json_get_int ( element, "num" );
             if ( 0 <= num && num < Agent_vars->nbr_entree_ana )
              { Agent_vars->AI[num] = element;
                Json_add_double ( Agent_vars->AI[num], "valeur", 0.0 );
                Json_add_bool   ( Agent_vars->AI[num], "in_range", FALSE );
                Json_add_bool   ( Agent_vars->AI[num], "need_sync", TRUE );
                Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_NOTICE, "New AI '%s' (%s, %s)",
                      Json_get_string ( Agent_vars->AI[num], "agent_acronyme" ),
                      Json_get_string ( Agent_vars->AI[num], "libelle" ),
                      Json_get_string ( Agent_vars->AI[num], "unite" ) );
              } else Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_WARNING, "Map AI: num %d out of range '%d'",
                           num, Agent_vars->nbr_entree_ana );
           }
        }
       else Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_ALERT, "Memory Error for AI" );
     }
/***************************************************** Mapping des DigitalInput ***********************************************/
    Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_INFO, "Allocate %d DI", Agent_vars->nbr_entree_tor );
    if(Agent_vars->nbr_entree_tor)
     { Agent_vars->DI = g_try_malloc0( sizeof(JsonNode *) * Agent_vars->nbr_entree_tor );
       if (Agent_vars->DI)
        { JsonArray *array = Agent_config_get_array ( Agent, "DI" );
          for ( guint cpt = 0; cpt < json_array_get_length ( array ); cpt++ )
           { JsonNode *element = json_array_get_element ( array, cpt );
             gint num = Json_get_int ( element, "num" );
             if ( 0 <= num && num < Agent_vars->nbr_entree_tor )
              { Agent_vars->DI[num] = element;
                Json_add_bool ( Agent_vars->DI[num], "etat", FALSE );
                Json_add_bool ( Agent_vars->DI[num], "need_sync", TRUE );
                Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_NOTICE, "New DI '%s' (%s), flip=%d",
                      Json_get_string ( Agent_vars->DI[num], "agent_acronyme" ),
                      Json_get_string ( Agent_vars->DI[num], "libelle" ),
                      Json_get_bool   ( Agent_vars->DI[num], "flip" ));
              } else Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_WARNING, "Map DI: num %d out of range '%d'",
                                num, Agent_vars->nbr_entree_tor );
           }
        }
       else Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_ALERT, "Memory Error for DI" );
     }
/***************************************************** Mapping des AnalogOutput ***********************************************/
    Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_INFO, "Allocate %d AO", Agent_vars->nbr_sortie_ana );
    if(Agent_vars->nbr_sortie_ana)
     { Agent_vars->AO = g_try_malloc0( sizeof(JsonNode *) * Agent_vars->nbr_sortie_ana );
       if (Agent_vars->AO)
        { JsonArray *array = Agent_config_get_array ( Agent, "AO" );
          for ( guint cpt = 0; cpt < json_array_get_length ( array ); cpt++ )
           { JsonNode *element = json_array_get_element ( array, cpt );
             gint num = Json_get_int ( element, "num" );
             if ( 0 <= num && num < Agent_vars->nbr_sortie_ana )
              { Agent_vars->AO[num] = element;
                Json_add_double ( Agent_vars->AO[num], "valeur", 0.0 );
                Json_add_int    ( Agent_vars->AO[num], "val_int", 0 );
                Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_NOTICE, "New AO '%s' (%s, %s)",
                      Json_get_string ( Agent_vars->AO[num], "agent_acronyme" ),
                      Json_get_string ( Agent_vars->AO[num], "libelle" ),
                      Json_get_string ( Agent_vars->AO[num], "unite" ) );
              } else Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_WARNING, "map AO: num %d out of range '%d'",
                               num, Agent_vars->nbr_sortie_ana );
           }
        }
       else Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_ALERT, "Memory Error for AO" );
     }
/***************************************************** Mapping des DigitalOutput **********************************************/
    Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_INFO, "Allocate %d DO", Agent_vars->nbr_sortie_tor );
    if(Agent_vars->nbr_sortie_tor)
     { Agent_vars->DO = g_try_malloc0( sizeof(JsonNode *) * Agent_vars->nbr_sortie_tor );
       if (Agent_vars->DO)
        { JsonArray *array = Agent_config_get_array ( Agent, "DO" );
          for ( guint cpt = 0; cpt < json_array_get_length ( array ); cpt++ )
           { JsonNode *element = json_array_get_element ( array, cpt );
             gint num = Json_get_int ( element, "num" );
             if ( 0 <= num && num < Agent_vars->nbr_sortie_tor )
              { Agent_vars->DO[num] = element;
                Json_add_bool   ( Agent_vars->DO[num], "etat", FALSE );
                Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_NOTICE, "New DO '%s' (%s)",
                      Json_get_string ( Agent_vars->DO[num], "agent_acronyme" ),
                      Json_get_string ( Agent_vars->DO[num], "libelle" ));
              } else Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_WARNING, "map DO: num %d out of range '%d'",
                               num, Agent_vars->nbr_sortie_tor );
           }
        }
       else Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_ALERT, " Memory Error for DO" );
     }
/******************************* Recherche des event text EA a raccrocher aux bits internes ***********************************/
    Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_NOTICE, "Module '%s' : io config done", Agent_vars->description );
  }
/******************************************************************************************************************************/
/* Recuperer_borne: Recupere les informations d'une borne MODBUS                                                              */
/* Entrée: identifiants des modules et borne                                                                                  */
/* Sortie: ?                                                                                                                  */
/******************************************************************************************************************************/
 static void Modbus_Processer_trame( void )
  { Agent_vars->nbr_oct_lu = 0;
    Agent_vars->request = FALSE;                                                                 /* Une requete a été traitée */

    if ( (guint16) Agent_vars->response.proto_id )
       { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_WARNING, "Wrong proto_id" );
         Deconnecter_module();
       }

    gint cpt_byte, cpt_poid, cpt;
    Agent_vars->top_last_response = Top_set_now();                                                 /* Estampillage de la date */
    Agent_send_comm_to_master ( Agent, TRUE );
    if (ntohs(Agent_vars->response.transaction_id) != Agent_vars->transaction_id)                         /* Mauvaise reponse */
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_ERR, "Wrong transaction_id: attendu %d, recu %d",
             Agent_vars->transaction_id, ntohs(Agent_vars->response.transaction_id) );
     }
    if ( Agent_vars->response.fct >=0x80 )
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_ERR, "Erreur Reponse, Error %d, Exception code %d",
             Agent_vars->response.fct, (int)Agent_vars->response.data[0] );
       Deconnecter_module();
       return;
     }
    switch (Agent_vars->mode)
     { case MODBUS_GET_DI:
            for ( cpt_poid = 1, cpt_byte = 1, cpt = 0; cpt<Agent_vars->nbr_entree_tor; cpt++)
             { if (Agent_vars->DI[cpt])                                                                   /* Si l'entrée est mappée */
                { gint new_etat_int = (Agent_vars->response.data[ cpt_byte ] & cpt_poid);
                  gboolean new_etat = (new_etat_int ? TRUE : FALSE);
                  if ( Json_get_bool ( Agent_vars->DI[cpt], "flip" ) ) new_etat = new_etat ^ 1;
                  Mqtt_Send_DI ( Agent, Agent_vars->DI[cpt], new_etat );
                }
               cpt_poid = cpt_poid << 1;
               if (cpt_poid == 256) { cpt_byte++; cpt_poid = 1; }
             }
            Agent_vars->mode = MODBUS_GET_AI;
            break;
       case MODBUS_GET_AI:
            for ( cpt = 0; cpt<Agent_vars->nbr_entree_ana; cpt++)
             { if (Agent_vars->AI[cpt])                                                                   /* Si l'entrée est mappée */
                { gint type_borne = Json_get_int ( Agent_vars->AI[cpt], "type_borne" );
                  gboolean new_in_range;
                  gdouble new_valeur;
                  switch( type_borne )
                   { case WAGO_750455:                                                                       /* Borne 4/20 mA */
                      { gint16 new_valeur_int  = (gint16)Agent_vars->response.data[ 2*cpt + 1 ] << 5;
                               new_valeur_int |= (gint16)Agent_vars->response.data[ 2*cpt + 2 ] >> 3;
                        gdouble min  = Json_get_double ( Agent_vars->AI[cpt], "min" );
                        gdouble max  = Json_get_double ( Agent_vars->AI[cpt], "max" );
                        new_valeur   = ((gint)new_valeur_int*(max - min))/4095.0 + min;
                        new_in_range = !(Agent_vars->response.data[ 2*cpt + 2 ] & 0x03);
                        break;
                      }
                     case WAGO_750461:                                                                         /* Borne PT100 */
                      { gint16 new_valeur_int  = (gint16)Agent_vars->response.data[ 2*cpt + 1 ] << 8;
                               new_valeur_int |= (gint16)Agent_vars->response.data[ 2*cpt + 2 ];
                        new_valeur  = ((gint)new_valeur_int)/10.0;
                        if (new_valeur_int > -2000 && new_valeur_int < 8500) new_in_range = TRUE; else new_in_range = FALSE;
                        break;
                      }
                     default : new_valeur=0.0; new_in_range=FALSE;
                   }
                  Mqtt_Send_AI ( Agent, Agent_vars->AI[cpt], new_valeur, new_in_range );
                }
             }
            Agent_vars->mode = MODBUS_SET_DO;
            break;
       case MODBUS_SET_DO:
            Agent_vars->mode = MODBUS_SET_AO;
            break;
       case MODBUS_SET_AO:
            Agent_vars->mode = MODBUS_GET_DI;
            break;
       case MODBUS_GET_DESCRIPTION:
          { gchar chaine[32];
            guint taille;
            memset ( chaine, 0, sizeof(chaine) );
            taille = Agent_vars->response.data[0];
            if (taille>=sizeof(chaine)) taille=sizeof(chaine)-1;
            chaine[0] = ntohs( (gint16)Agent_vars->response.data[1] );
            chaine[2] = ntohs( (gint16)Agent_vars->response.data[3] );
            chaine[taille] = 0;
            Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_INFO, "Description (size %d) = '%s'", taille, chaine );
            Agent_vars->mode = MODBUS_GET_FIRMWARE;
            break;
         }
       case MODBUS_GET_FIRMWARE:
          { gchar chaine[64];
            guint taille;
            memset ( chaine, 0, sizeof(chaine) );
            taille = Agent_vars->response.data[0];
            if (taille>=sizeof(chaine)) taille=sizeof(chaine)-1;
            chaine[0] = ntohs( (gint16)Agent_vars->response.data[1] );
            chaine[2] = ntohs( (gint16)Agent_vars->response.data[3] );
            chaine[taille] = 0;
            Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_INFO, "Firmware (size %d) = '%s'", taille, chaine );
            Agent_vars->mode = MODBUS_INIT_WATCHDOG1;
            break;
         }
       case MODBUS_INIT_WATCHDOG1:
            Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_DEBUG, "Watchdog1 = %d %d",
                  ntohs( *(gint16 *)((gchar *)&Agent_vars->response.data + 0) ),
                  ntohs( *(gint16 *)((gchar *)&Agent_vars->response.data + 2) )
                );
            Agent_vars->mode = MODBUS_INIT_WATCHDOG2;
            break;
       case MODBUS_INIT_WATCHDOG2:
            Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_DEBUG, "Watchdog2 = %d %d",
                  ntohs( *(gint16 *)((gchar *)&Agent_vars->response.data + 0) ),
                  ntohs( *(gint16 *)((gchar *)&Agent_vars->response.data + 2) )
                );
            Agent_vars->mode = MODBUS_INIT_WATCHDOG3;
            break;
       case MODBUS_INIT_WATCHDOG3:
            Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_DEBUG, "Watchdog3 = %d %d",
                  ntohs( *(gint16 *)((gchar *)&Agent_vars->response.data + 0) ),
                  ntohs( *(gint16 *)((gchar *)&Agent_vars->response.data + 2) )
                );
            Agent_vars->mode = MODBUS_INIT_WATCHDOG4;
            break;
       case MODBUS_INIT_WATCHDOG4:
            Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_DEBUG, "Watchdog4 = %d %d",
                  ntohs( *(gint16 *)((gchar *)&Agent_vars->response.data + 0) ),
                  ntohs( *(gint16 *)((gchar *)&Agent_vars->response.data + 2) )
                );
            Agent_vars->mode = MODBUS_GET_NBR_AI;
            break;
       case MODBUS_GET_NBR_AI:
             { Agent_vars->nbr_entree_ana = ntohs( *(gint16 *)((gchar *)&Agent_vars->response.data + 1) ) / 16;
               Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_INFO, "Get %03d Entree ANA", Agent_vars->nbr_entree_ana );
               Agent_vars->mode = MODBUS_GET_NBR_AO;
             }
            break;
       case MODBUS_GET_NBR_AO:
             { Agent_vars->nbr_sortie_ana = ntohs( *(gint16 *)((gchar *)&Agent_vars->response.data + 1) ) / 16;
               Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_INFO, "Get %03d Sortie ANA", Agent_vars->nbr_sortie_ana );
               Agent_vars->mode = MODBUS_GET_NBR_DI;
             }
            break;
       case MODBUS_GET_NBR_DI:
             { gint nbr;
               nbr = ntohs( *(gint16 *)((gchar *)&Agent_vars->response.data + 1) );
               Agent_vars->nbr_entree_tor = nbr;
               Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_INFO, "Get %03d Entree TOR", Agent_vars->nbr_entree_tor );
               Agent_vars->mode = MODBUS_GET_NBR_DO;
             }
            break;
       case MODBUS_GET_NBR_DO:
             { Agent_vars->nbr_sortie_tor = ntohs( *(gint16 *)((gchar *)&Agent_vars->response.data + 1) );
               Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_INFO, "Get %03d Sortie TOR", Agent_vars->nbr_sortie_tor );
               Modbus_load_io_config();                                                          /* Initialise les IO modules */
               JsonNode *RootNode = Json_create();                                                /* Envoi de la conf a l'API */
               if (!RootNode) break;
               Json_add_string ( RootNode, "agent_tech_id", Agent->agent_tech_id );
               Json_add_int    ( RootNode, "nbr_entree_tor", Agent_vars->nbr_entree_tor );
               Json_add_int    ( RootNode, "nbr_entree_ana", Agent_vars->nbr_entree_ana );
               Json_add_int    ( RootNode, "nbr_sortie_tor", Agent_vars->nbr_sortie_tor );
               Json_add_int    ( RootNode, "nbr_sortie_ana", Agent_vars->nbr_sortie_ana );
               JsonNode *API_result = Http_Post_to_global_API ( Agent, "/run/modbus/add/io", RootNode );
               Json_unref ( API_result );
               Json_unref ( RootNode );
               Agent_vars->mode = MODBUS_GET_DI;
             }
            break;
     }
  }
/******************************************************************************************************************************/
/* Recuperer_borne: Recupere les informations d'une borne MODBUS                                                              */
/* Entrée: identifiants des modules et borne                                                                                  */
/* Sortie: ?                                                                                                                  */
/******************************************************************************************************************************/
 static void Recuperer_reponse_module( void )
  { fd_set fdselect;
    struct timeval tv;
    gint retval, cpt;

    if ( Top_is_too_old(Agent_vars->top_last_response, 60 ) )                                /* Detection attente trop longue */
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_WARNING,
             "Timeout agent started=%d, mode=%02d, "
             "transactionID=%06d, nbr_deconnect=%02d, top_last_response=%03ds ago, "
             "top_next_reconnect=in %03ds, top_next_eana=in %03ds",
             Agent_vars->started, Agent_vars->mode, Agent_vars->transaction_id, Agent_vars->nbr_deconnect,
             Top_age   (Agent_vars->top_last_response),
             Top_until (Agent_vars->top_next_reconnect),
             Top_until (Agent_vars->top_next_eana)
           );
       Deconnecter_module();
       return;
     }

    FD_ZERO(&fdselect);
    FD_SET(Agent_vars->connexion, &fdselect);
    tv.tv_sec = 0;
    tv.tv_usec= 1000;                                                                               /* Attente d'un caractere */
    retval = select(Agent_vars->connexion+1, &fdselect, NULL, NULL, &tv );

    if ( retval>0 && FD_ISSET(Agent_vars->connexion, &fdselect) )
     { guint bute;
       if (Agent_vars->nbr_oct_lu<TAILLE_ENTETE_MODBUS)
            { bute = TAILLE_ENTETE_MODBUS; }
       else { bute = TAILLE_ENTETE_MODBUS + ntohs(Agent_vars->response.taille); }

       if (bute>=sizeof(struct TRAME_MODBUS_REPONSE))
        { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_ERR,
                "bute = %d >= %d (sizeof(agent->reponse)=%d, taille recue = %d)",
                bute, sizeof(struct TRAME_MODBUS_REPONSE), sizeof(Agent_vars->response), ntohs(Agent_vars->response.taille) );
          Deconnecter_module();
          return;
        }

       cpt = read( Agent_vars->connexion, (unsigned char *)&Agent_vars->response + Agent_vars->nbr_oct_lu, bute-Agent_vars->nbr_oct_lu );
       if (cpt>=0)
        { Agent_vars->nbr_oct_lu += cpt;
          if (Agent_vars->nbr_oct_lu >= TAILLE_ENTETE_MODBUS + ntohs(Agent_vars->response.taille))
           { Modbus_Processer_trame(); }                                            /* Si l'on a trouvé une trame complète !! */
        }
       else
        { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_WARNING, "Read Error. Get %d, error %s", cpt, strerror(errno) );
          Deconnecter_module ();
        }
      }
  }
/******************************************************************************************************************************/
/* main: Point d'entrée de l'agent modbus                                                                                     */
/* Entrée: argc/argv                                                                                                          */
/* Sortie: code de retour process                                                                                             */
/******************************************************************************************************************************/
 gint main ( gint argc, gchar *argv[] )
  { Config_add_parameter ( "hostname",    "host",   "Host du device Modbus",                CONFIG_STRING );
    Config_add_parameter ( "description", "string", "Description du device Modbus",         CONFIG_STRING );
    Config_add_parameter ( "watchdog",    "int",    "Délai de sécurité en 1/10 de seconde", CONFIG_INT    );
    Agent = Agent_init ( argv[0], "modbus", ABLS_AGENT_MODBUS_VERSION, sizeof(struct MODBUS_VARS), argc, argv );
    Agent_vars = Agent->vars;

    Agent_vars->hostname = Agent_config_get_string ( Agent, "hostname" );
    if (!Agent_vars->hostname)
     { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_ERR, "Missing hostname, stopping." );
       Agent_end ( Agent );
     }
    Agent_vars->watchdog = Agent_config_get_int ( Agent, "watchdog" );
    Agent_vars->description = Agent_config_get_string ( Agent, "description" );

    Mqtt_subscribe ( Agent->mqtt_local, "SYNC_INPUT/%s", Agent->agent_tech_id );
    Agent_is_ready ( Agent );

    while(Agent->Agent_run == AGENT_IS_RUNNING)                                             /* On tourne tant que necessaire */
     { Agent_loop ( Agent );                                            /* Loop sur l'Agent pour mettre a jour la telemetrie */
/****************************************************** Ecoute du master ******************************************************/
       JsonNode *mqtt_local_message;
       while ( (mqtt_local_message = Agent_get_mqtt_local_message(Agent)) != NULL )
        { if ( Mqtt_topic_is(mqtt_local_message, 2, "SET_DO", Agent->agent_tech_id) )
           { Modbus_SET_DO ( mqtt_local_message ); }
          else if ( Mqtt_topic_is(mqtt_local_message, 2, "SET_AO", Agent->agent_tech_id) )
           { Modbus_SET_AO ( mqtt_local_message ); }
          else if ( Mqtt_topic_is(mqtt_local_message, 2, "SYNC_INPUT", Agent->agent_tech_id) )
           { Modbus_Sync_INPUT_to_master (); }
          Json_unref ( mqtt_local_message );
        }

/****************************************************** Ecoute de l'api *******************************************************/
       JsonNode *mqtt_api_message;
       while ( (mqtt_api_message = Agent_get_mqtt_api_message(Agent)) != NULL )
        { if ( Mqtt_topic_is ( mqtt_api_message, 4, "+", "AGENT", Agent->agent_tech_id, "TEST" ) )
           { Info(__func__, Agent->agent_classe, Agent->agent_tech_id, LOG_NOTICE, "Agent Test from API."); }
          Json_unref ( mqtt_api_message );
        }

/********************************************* Début de l'interrogation du module *********************************************/
       if ( Agent_vars->started == FALSE )                                           /* Si non started, on tente la connexion */
        { if ( Top_is_out(Agent_vars->top_next_reconnect) && Connecter_module()==FALSE )
           { Info( __func__, Agent->agent_classe, Agent->agent_tech_id, LOG_INFO,
                   "Module DOWN. retrying in %ds", MODBUS_TOP_NEXT_RETRY );
             Agent_vars->top_next_reconnect = Top_set_next_in(MODBUS_TOP_NEXT_RETRY);
           }
        }
       else for (gint i=0; i<4; i++)                                     /* 1 tour programme = 4 itérations GET (DI/DO/AI/AO) */
        { if ( Agent_vars->request )                                                     /* Requete en cours pour ce module ? */
           { Recuperer_reponse_module(); }
          else
           { if (Top_is_out(Agent_vars->top_next_eana))                                /* Gestion décalée des I/O Analogiques */
              { Agent_vars->top_next_eana = Top_set_next_in(MODBUS_TOP_NEXT_EANA);                       /* Tous les secondes */
                Agent_vars->do_check_eana = TRUE;
              }
             switch (Agent_vars->mode)
              { case MODBUS_GET_DESCRIPTION: Interroger_description(); break;
                case MODBUS_GET_FIRMWARE   : Interroger_firmware(); break;
                case MODBUS_INIT_WATCHDOG1 : Init_watchdog1(); break;
                case MODBUS_INIT_WATCHDOG2 : Init_watchdog2(); break;
                case MODBUS_INIT_WATCHDOG3 : Init_watchdog3(); break;
                case MODBUS_INIT_WATCHDOG4 : Init_watchdog4(); break;
                case MODBUS_GET_NBR_AI     : Interroger_nbr_entree_ANA(); break;
                case MODBUS_GET_NBR_AO     : Interroger_nbr_sortie_ANA(); break;
                case MODBUS_GET_NBR_DI     : Interroger_nbr_entree_TOR(); break;
                case MODBUS_GET_NBR_DO     : Interroger_nbr_sortie_TOR(); break;
                case MODBUS_GET_DI         : if (Agent_vars->nbr_entree_tor) Interroger_entree_tor();
                                             else Agent_vars->mode = MODBUS_GET_AI;
                                             break;
                case MODBUS_GET_AI         : if (Agent_vars->nbr_entree_ana && Agent_vars->do_check_eana)
                                              { Interroger_entree_ana(); }
                                             else Agent_vars->mode = MODBUS_SET_DO;
                                             break;
                case MODBUS_SET_DO         : if (Agent_vars->nbr_sortie_tor) Interroger_sortie_tor();
                                             else Agent_vars->mode = MODBUS_SET_AO;
                                             break;
                case MODBUS_SET_AO         : if (Agent_vars->nbr_sortie_ana && Agent_vars->do_check_eana)
                                              { Interroger_sortie_ana(); }
                                             else Agent_vars->mode = MODBUS_GET_DI;
                                             Agent_vars->do_check_eana = FALSE;                                /* Le check est fait */
                                             break;
              }
           }
        }
     }

    Deconnecter_module();
    Agent_end(Agent);
    return 0;
  }
/*----------------------------------------------------------------------------------------------------------------------------*/
