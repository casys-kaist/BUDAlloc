<?php
$d = new \DOMDocument();
$d->appendChild($d->createElement("html"));
$a = $d->createAttributeNS("fake_ns", "test:test");
$d->removeChild($d->documentElement);
echo $a->namespaceURI;
echo $a->prefix;