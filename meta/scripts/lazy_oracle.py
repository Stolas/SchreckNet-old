#!/usr/bin/env python

import requests
import json
from xml.dom import minidom

discipline_archive = {}
discipline_archive['abo'] = "Abombwe"
discipline_archive['ani'] = "Animalism"
discipline_archive['aus'] = "Auspex"
discipline_archive['cel'] = "Celerity"
discipline_archive['chi'] = "Chimerstry"
discipline_archive['dai'] = "Daimoinon"
discipline_archive['def'] = "Defense"
discipline_archive['dem'] = "Dementation"
discipline_archive['dom'] = "Dominate"
discipline_archive['for'] = "Fortitude"
discipline_archive['inn'] = "Innocence"
discipline_archive['jud'] = "Judgment"
discipline_archive['mar'] = "Martyrdom"
discipline_archive['mel'] = "Melpominee"
discipline_archive['myt'] = "Mytherceria"
discipline_archive['nec'] = "Necromancy"
discipline_archive['obe'] = "Obeah"
discipline_archive['obf'] = "Obfuscate"
discipline_archive['obt'] = "Obtenebration"
discipline_archive['pot'] = "Potence"
discipline_archive['pre'] = "Presence"
discipline_archive['pro'] = "Protean"
discipline_archive['qui'] = "Quietus"
discipline_archive['red'] = "Redemption"
discipline_archive['san'] = "Sanguinus"
discipline_archive['ser'] = "Serpentice"
discipline_archive['spi'] = "Spiritus"
discipline_archive['tem'] = "Temporis"
discipline_archive['tha'] = "Blood Sorcery" # Traumatery
discipline_archive['thn'] = "Thanatosis"
discipline_archive['val'] = "Valeren"
discipline_archive['ven'] = "Vengeance"
discipline_archive['vic'] = "Vicissitude"
discipline_archive['vis'] = "Visceratika"
discipline_archive['viz'] = "Vision"


def simple_json_to_xml(root, card, key, name):
    elem = root.createElement(key)
    for x in card['properties'].get(key, []):
        new_elem = root.createElement(name)
        new_elem.setAttribute("name", x)
        elem.appendChild(new_elem)
    return elem

def create_card(root, card):
    new_card = root.createElement('card')
    new_card.setAttribute('id', str(card['id']))
    new_card.setAttribute('name', card['name'])
    new_card.setAttribute('picture', card['picture_url'])

    text_element = root.createElement('text')
    text = root.createTextNode('text')
    card_text = card['text']
    card_text = card_text.replace("\n", "\\n")
    text.data = card_text.encode("utf-8").decode()
    text_element.appendChild(text)
    new_card.appendChild(text_element)

    properties = root.createElement('properties')

    # Crypt / Library Specific
    try:
        if card['type'] == 'crypt':
            properties.setAttribute('capacity', str(card['properties']['capacity']))
            properties.setAttribute('group', str(card['properties']['group']))
            properties.setAttribute('title', str(card['properties']['title']))
        else:
            properties.setAttribute('pool', str(card['properties']['pool_cost']))
            properties.setAttribute('blood', str(card['properties']['blood_cost']))
    except KeyError:
        # Likely a token
        pass

    # Types, clans
    properties.appendChild(simple_json_to_xml(root, card, 'types', 'type'))
    properties.appendChild(simple_json_to_xml(root, card, 'clans', 'clan'))

    # Disciplines
    disciplines = root.createElement('disciplines')
    try:
        for discipline_ in card['properties']['disciplines']:
            discipline = discipline_.lower()
            try:
                discipline_name = discipline_archive[discipline]
            except KeyError:
                exp = f"Unknown Disciplines: {discipline} for card {card['name']}"
                print(f"[!] {exp}")
                raise RuntimeError(exp)

            dd = root.createElement("discipline")
            dd.setAttribute("name", discipline_name)
            if discipline.upper() == discipline_:
                dd.setAttribute("level", "inferior")
            else:
                dd.setAttribute("level", "superior")
            disciplines.appendChild(dd)

        properties.appendChild(disciplines)
    except TypeError as exp:
        pass # NO Disciplines
    except KeyError:
        pass

    # Sets
    sets = root.createElement('sets')
    for set_ in card['properties'].get('sets',[]):
        set_info = card['properties']['sets'][set_][0]
        ss = root.createElement("set")
        ss.setAttribute("name", set_)
        ss.setAttribute("rarity", set_info.get('rarity', 'POD'))
        #ss.setAttribute("frequency", set_info['frequency'])
        sets.appendChild(ss)
    properties.appendChild(sets)

    # Generic

    new_card.appendChild(properties)
    return new_card

def card_db_to_xml(root, card_db, crypt, library):
    for card in card_db:
        if card['type'] == 'crypt':
            card_element = crypt
        else:
            card_element = library

        new_card = create_card(root, card)
        card_element.appendChild(new_card)

def extract_sets(card_db):
    card_sets = {}
    for card in card_db:
        for k in card['properties']['sets']:
            release_date = card['properties']['sets'][k][0].get("release_date", 'Unspecified')
            card_sets[k] = release_date
    return card_sets

def create_schrecknetxml(card_db):
    root = minidom.Document()
    xml = root.createElement('schrecknetxml')

    crypt_cards = root.createElement('crypt_cards')
    library_cards = root.createElement('library_cards')
    card_db_to_xml(root, card_db, crypt_cards, library_cards)

    tokens = root.createElement('tokens')
    tokens.appendChild(create_card(root, {"id": 0, "name": "The Edge", "picture_url": "xxx", "text": "", "properties": {}}))

    sets = root.createElement('sets')
    card_sets = extract_sets(card_db)
    for set_ in card_sets:
        set_elem = root.createElement('set')
        set_elem.setAttribute('name', set_)
        set_elem.setAttribute('release_date', card_sets[set_])
        sets.appendChild(set_elem)

    root.appendChild(xml)
    xml.appendChild(crypt_cards)
    xml.appendChild(library_cards)
    xml.appendChild(tokens)
    xml.appendChild(sets)

    with open("schrecknet.xml", "wb") as fd:
        fd.write(root.toprettyxml(indent ="\t", encoding="utf-16"))

def add_card(card_id, card_type, name, picture_url, text, properties):
    # Todo; extra parsing of the card text here.
    return {"id": card_id, "type": card_type, "name": name, "text": text, "properties": properties, "picture_url": picture_url}

def load_json():
    URI = "https://static.krcg.org/data/vtes.json"
    resp = requests.get(URI)
    if resp.status_code != 200:
        print(f"[!] Failed, KRCG returned: {resp.status_code}")
        return

    import ipdb ; ipdb.set_trace()
    all_cards = []
    for card in resp.json():
        properties = {}
        if "Vampire" in card['types'] or "Imbued" in card['types']:
            card_type = "crypt"
            # Parse as Crypt Card
            properties["capacity"] = card["capacity"]
            properties["disciplines"] = card.get("disciplines", None)
            properties["group"] = card["group"]
            properties["title"] = card.get("title", '')
            properties["advanced"] = card.get("adv", False)
        else:
            # Parse as Library Card
            card_type = "library"
            properties["pool_cost"] = card.get("pool_cost",0)
            properties["blood_cost"] = card.get("blood_cost",0)

        card_id = card["id"]
        name = card["_name"]
        text = card["card_text"];
        picture_url = card["url"];
        properties["clans"] = card.get("clans", [])
        properties["types"] = card.get("types", [])
        properties["sets"] = card["sets"];

        all_cards.append(add_card(card_id, card_type, name, picture_url, text, properties))
    return all_cards

if __name__ == '__main__':
    all_cards = load_json()
    create_schrecknetxml(all_cards)
